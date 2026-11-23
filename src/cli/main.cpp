#include "cli/cli_common.hpp"
#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncons/json.hpp>
#include <libxml/parser.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

namespace fs = std::filesystem;

enum class BatchFormat { Ndjson, JsonArray, JsonObject, OutDir };

struct CliOptions {
  std::string dsl_path;
  std::string scraper_name;
  std::string selector;
  std::string html_input;
  bool has_html = false;
  bool compact = false;

  std::vector<std::string> batch_files;
  bool has_batch = false;
  BatchFormat batch_format = BatchFormat::Ndjson;
  bool has_batch_format = false;
  std::string out_dir;
  int threads = 1;
};

struct BatchResult {
  std::string file;
  jsoncons::json data;
  std::string error;
};

void print_usage() {
  std::cout
      << "xsel2 - High-Performance HTML/XML Scraping CLI\n\n"
      << "Usage:\n"
      << "  xsel2 (-d <file> [-n <scraper>] | -s <css>) [--html <file|str|->] [-c]\n"
      << "  xsel2 (-d <file> [-n <scraper>] | -s <css>) --batch <file1> [<file2> ...]\n"
      << "        [--ndjson | --json-array | --json-object | --out-dir <dir>] [-j <N>] [-c]\n\n"
      << "Options:\n"
      << "  -d, --dsl <file>       Path to a .dsl file\n"
      << "  -n, --name <scraper>   Scraper name in the .dsl file (runs all if omitted)\n"
      << "  -s, --selector <css>   Quick single-selector extraction\n"
      << "      --html <input>     HTML file, inline string, or '-' for stdin (defaults to stdin)\n"
      << "  -c, --compact          Emit compact single-line JSON\n"
      << "      --batch <files...> Process one or more HTML/XML files\n"
      << "      --ndjson           Batch output: one JSON line per file (default)\n"
      << "      --json-array       Batch output: single JSON array\n"
      << "      --json-object      Batch output: single JSON object keyed by file path\n"
      << "      --out-dir <dir>    Batch output: write <dir>/<filename>.json per file\n"
      << "  -j, --threads <N>      Worker threads for --batch (default: 1, 0 = auto)\n"
      << "  -h, --help             Print this help message\n";
}

std::string read_stream(std::istream &in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string read_file_strict(const std::string &path) {
  std::ifstream ifs{path};
  if (!ifs.is_open()) {
    throw std::runtime_error("Cannot open file: " + path);
  }
  return read_stream(ifs);
}

std::string read_arg_or_file(std::string_view value) {
  if (value == "-") {
    return read_stream(std::cin);
  }
  std::ifstream probe{std::string(value)};
  if (probe.is_open()) {
    return read_stream(probe);
  }
  return std::string(value);
}

std::string require_flag_value(const std::vector<std::string_view> &args,
                               size_t &i) {
  if (i + 1 >= args.size()) {
    throw std::runtime_error("Missing value for flag: " +
                             std::string(args[i]));
  }
  return std::string(args[++i]);
}

int parse_non_negative_int(std::string_view s, std::string_view flag) {
  int val = 0;
  const char *begin = s.data();
  const char *end = begin + s.size();
  auto [ptr, ec] = std::from_chars(begin, end, val);
  if (ec != std::errc{} || ptr != end || val < 0) {
    throw std::runtime_error("Invalid non-negative integer for " +
                             std::string(flag) + ": " + std::string(s));
  }
  return val;
}

void set_batch_format(CliOptions &opts, BatchFormat fmt,
                      std::string_view flag) {
  if (opts.has_batch_format) {
    throw std::runtime_error("Conflicting batch output format flag: " +
                             std::string(flag));
  }
  opts.batch_format = fmt;
  opts.has_batch_format = true;
}

CliOptions parse_args(const std::vector<std::string_view> &args) {
  CliOptions opts;
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "-d" || arg == "--dsl") {
      opts.dsl_path = require_flag_value(args, i);
    } else if (arg == "-n" || arg == "--name") {
      opts.scraper_name = require_flag_value(args, i);
    } else if (arg == "-s" || arg == "--selector") {
      opts.selector = require_flag_value(args, i);
    } else if (arg == "--html") {
      opts.html_input = require_flag_value(args, i);
      opts.has_html = true;
    } else if (arg == "-c" || arg == "--compact") {
      opts.compact = true;
    } else if (arg == "--batch") {
      opts.has_batch = true;
      while (i + 1 < args.size() && !args[i + 1].starts_with('-')) {
        opts.batch_files.emplace_back(args[++i]);
      }
    } else if (arg == "--ndjson") {
      set_batch_format(opts, BatchFormat::Ndjson, arg);
    } else if (arg == "--json-array") {
      set_batch_format(opts, BatchFormat::JsonArray, arg);
    } else if (arg == "--json-object") {
      set_batch_format(opts, BatchFormat::JsonObject, arg);
    } else if (arg == "--out-dir") {
      set_batch_format(opts, BatchFormat::OutDir, arg);
      opts.out_dir = require_flag_value(args, i);
    } else if (arg == "-j" || arg == "--threads") {
      opts.threads = parse_non_negative_int(require_flag_value(args, i), arg);
    } else {
      throw std::runtime_error("Unknown flag: " + std::string(arg));
    }
  }
  return opts;
}

void validate_options(const CliOptions &opts) {
  if (!opts.dsl_path.empty() && !opts.selector.empty()) {
    throw std::runtime_error("--dsl and --selector are mutually exclusive.");
  }
  if (opts.dsl_path.empty() && opts.selector.empty()) {
    throw std::runtime_error("--dsl (-d) or --selector (-s) is required.");
  }
  if (opts.has_html && opts.has_batch) {
    throw std::runtime_error("--html and --batch are mutually exclusive.");
  }
  if (opts.has_batch && opts.batch_files.empty()) {
    throw std::runtime_error("--batch requires at least one input file.");
  }
  if (!opts.has_batch && (opts.has_batch_format || opts.threads != 1)) {
    throw std::runtime_error(
        "Batch output/thread flags require --batch.");
  }
}

/// Holds either a compiled selector pipeline or parsed DSL scrapers so each
/// worker thread can own an independent copy without sharing mutable state.
struct ScraperTemplate {
  bool use_selector = false;
  scraper::ScraperBuilder selector_builder;
  absl::flat_hash_map<std::string, scraper::ScraperBuilder> dsl_scrapers;
  std::string scraper_name;

  jsoncons::json run(std::string_view html) {
    if (use_selector) {
      return selector_builder.scrape_from_string(html);
    }
    return xsel::cli::run_dsl_scrapers(dsl_scrapers, scraper_name, html);
  }
};

ScraperTemplate build_template(const CliOptions &opts) {
  ScraperTemplate tpl;
  tpl.scraper_name = opts.scraper_name;
  if (!opts.selector.empty()) {
    tpl.use_selector = true;
    tpl.selector_builder.Select(opts.selector, "matches")
        .Text()
        .Squish()
        .Set("text");
    return tpl;
  }

  scraper::dsl::DslParser parser;
  tpl.dsl_scrapers = parser.parse_file(opts.dsl_path);
  if (tpl.dsl_scrapers.empty()) {
    throw std::runtime_error("No scrapers found in DSL file.");
  }
  if (!opts.scraper_name.empty() &&
      !tpl.dsl_scrapers.contains(opts.scraper_name)) {
    throw std::runtime_error("Scraper '" + opts.scraper_name + "' not found.");
  }
  return tpl;
}

size_t resolve_worker_count(int threads_arg, size_t num_files) {
  size_t requested = static_cast<size_t>(threads_arg);
  if (requested == 0) {
    requested = std::thread::hardware_concurrency();
    if (requested == 0) {
      requested = 1;
    }
  }
  return std::min(requested, num_files);
}

int write_batch_results(const CliOptions &opts,
                        const std::vector<BatchResult> &results) {
  bool any_error = false;
  for (const auto &r : results) {
    if (!r.error.empty()) {
      any_error = true;
      break;
    }
  }

  switch (opts.batch_format) {
  case BatchFormat::Ndjson: {
    for (const auto &r : results) {
      jsoncons::json row = jsoncons::json::object();
      row.insert_or_assign("file", r.file);
      if (!r.error.empty()) {
        row.insert_or_assign("error", r.error);
      } else {
        row.insert_or_assign("result", r.data);
      }
      xsel::cli::print_json(row, true);
    }
    break;
  }
  case BatchFormat::JsonArray: {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &r : results) {
      jsoncons::json row = jsoncons::json::object();
      row.insert_or_assign("file", r.file);
      if (!r.error.empty()) {
        row.insert_or_assign("error", r.error);
      } else {
        row.insert_or_assign("result", r.data);
      }
      arr.push_back(std::move(row));
    }
    xsel::cli::print_json(arr, opts.compact);
    break;
  }
  case BatchFormat::JsonObject: {
    jsoncons::json obj = jsoncons::json::object();
    for (const auto &r : results) {
      if (!r.error.empty()) {
        jsoncons::json err_obj = jsoncons::json::object();
        err_obj.insert_or_assign("error", r.error);
        obj.insert_or_assign(r.file, std::move(err_obj));
      } else {
        obj.insert_or_assign(r.file, r.data);
      }
    }
    xsel::cli::print_json(obj, opts.compact);
    break;
  }
  case BatchFormat::OutDir: {
    fs::create_directories(opts.out_dir);
    for (const auto &r : results) {
      if (!r.error.empty()) {
        std::cerr << "Error [" << r.file << "]: " << r.error << '\n';
        continue;
      }
      const fs::path out_path =
          fs::path(opts.out_dir) /
          (fs::path(r.file).filename().string() + ".json");
      std::ofstream ofs{out_path};
      if (!ofs.is_open()) {
        std::cerr << "Error: Cannot write output file: " << out_path << '\n';
        any_error = true;
        continue;
      }
      if (opts.compact) {
        std::string out;
        r.data.dump(out);
        ofs << out << '\n';
      } else {
        ofs << jsoncons::pretty_print(r.data) << '\n';
      }
    }
    break;
  }
  }

  return any_error ? 1 : 0;
}

int run_batch(const CliOptions &opts, const ScraperTemplate &tpl) {
  const size_t n = opts.batch_files.size();
  const size_t num_workers = resolve_worker_count(opts.threads, n);

  if (num_workers > 1) {
    // Initialize libxml2 global parser state once before spawning worker threads.
    xmlInitParser();
  }

  std::vector<BatchResult> results(n);
  std::atomic<size_t> next_idx{0};

  auto worker = [&]() {
    ScraperTemplate local_tpl = tpl;
    while (true) {
      const size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= n) {
        break;
      }
      const std::string &path = opts.batch_files[idx];
      results[idx].file = path;
      try {
        const std::string html = read_file_strict(path);
        results[idx].data = local_tpl.run(html);
      } catch (const std::exception &e) {
        results[idx].error = e.what();
      }
    }
  };

  if (num_workers == 1) {
    worker();
  } else {
    std::vector<std::thread> pool;
    pool.reserve(num_workers);
    for (size_t t = 0; t < num_workers; ++t) {
      pool.emplace_back(worker);
    }
    for (auto &t : pool) {
      t.join();
    }
  }

  return write_batch_results(opts, results);
}

} // namespace

int main(int argc, char *argv[]) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  // On Windows, command line arguments in argv are encoded in ANSI / current ACP.
  // Convert Unicode arguments from GetCommandLineW to UTF-8 std::string.
  int win_argc = 0;
  LPWSTR *win_argv = CommandLineToArgvW(GetCommandLineW(), &win_argc);
  std::vector<std::string> utf8_args_storage;
  std::vector<std::string_view> args;
  if (win_argv) {
    utf8_args_storage.reserve(static_cast<size_t>(win_argc));
    args.reserve(static_cast<size_t>(win_argc));
    for (int i = 0; i < win_argc; ++i) {
      int len = WideCharToMultiByte(CP_UTF8, 0, win_argv[i], -1, nullptr, 0, nullptr, nullptr);
      if (len > 1) {
        std::string s(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, win_argv[i], -1, s.data(), len, nullptr, nullptr);
        utf8_args_storage.push_back(std::move(s));
      } else {
        utf8_args_storage.emplace_back();
      }
      args.emplace_back(utf8_args_storage.back());
    }
    LocalFree(win_argv);
  } else {
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }
  }
#else
  std::vector<std::string_view> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
#endif

  if (args.size() < 2) {
    print_usage();
    return 1;
  }
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-h" || args[i] == "--help") {
      print_usage();
      return 0;
    }
  }

  try {
    const CliOptions opts = parse_args(args);
    validate_options(opts);
    ScraperTemplate tpl = build_template(opts);

    if (opts.has_batch) {
      return run_batch(opts, tpl);
    }

    const std::string html =
        opts.has_html ? read_arg_or_file(opts.html_input) : read_stream(std::cin);
    xsel::cli::print_json(tpl.run(html), opts.compact);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
