#include "cli/cli_common.hpp"
#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <chrono>
#include <emscripten/bind.h>
#include <jsoncons/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string format_json(const jsoncons::json &doc, bool compact) {
  if (compact) {
    std::string out;
    doc.dump(out);
    return out;
  }
  std::ostringstream oss;
  oss << jsoncons::pretty_print(doc);
  return oss.str();
}

std::string make_envelope(bool ok, const std::string &output,
                          const std::string &error, double elapsed_ms) {
  jsoncons::json res = jsoncons::json::object();
  res.insert_or_assign("ok", ok);
  res.insert_or_assign("output", output);
  res.insert_or_assign("error", error);
  res.insert_or_assign("elapsedMs", elapsed_ms);
  std::string dumped;
  res.dump(dumped);
  return dumped;
}

std::string make_cli_envelope(bool ok, const std::string &stdout_str,
                              const std::string &stderr_str, int exit_code,
                              double elapsed_ms) {
  jsoncons::json res = jsoncons::json::object();
  res.insert_or_assign("ok", ok);
  res.insert_or_assign("stdout", stdout_str);
  res.insert_or_assign("stderr", stderr_str);
  res.insert_or_assign("exitCode", exit_code);
  res.insert_or_assign("elapsedMs", elapsed_ms);
  std::string dumped;
  res.dump(dumped);
  return dumped;
}

double elapsed_since(std::chrono::steady_clock::time_point start) {
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

/// Tokenizes a shell-like command line string into argv tokens.
std::vector<std::string> split_command_line(std::string_view cmd) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_token = false;
  char quote = '\0';

  for (size_t i = 0; i < cmd.size(); ++i) {
    const char c = cmd[i];
    if (quote != '\0') {
      if (c == quote) {
        quote = '\0';
      } else if (c == '\\' && quote == '"' && i + 1 < cmd.size()) {
        current.push_back(cmd[++i]);
      } else {
        current.push_back(c);
      }
      in_token = true;
      continue;
    }

    if (c == '\'' || c == '"') {
      quote = c;
      in_token = true;
    } else if (c == '\\' && i + 1 < cmd.size()) {
      current.push_back(cmd[++i]);
      in_token = true;
    } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (in_token) {
        tokens.push_back(std::move(current));
        current.clear();
        in_token = false;
      }
    } else {
      current.push_back(c);
      in_token = true;
    }
  }

  if (quote != '\0') {
    throw std::runtime_error("Unterminated quote in command line.");
  }
  if (in_token) {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

std::string cli_usage_text() {
  return "xsel2 (WebAssembly) - High-Performance HTML/XML Scraping CLI\n\n"
         "Usage:\n"
         "  xsel2 (-d <file> [-n <scraper>] | -s <css>) [--html <file|str|->] "
         "[-c]\n\n"
         "Options:\n"
         "  -d, --dsl <file>       DSL buffer (any filename maps to the DSL "
         "editor)\n"
         "  -n, --name <scraper>   Scraper name in the .dsl file (runs all if "
         "omitted)\n"
         "  -s, --selector <css>   Quick single-selector extraction\n"
         "      --html <input>     HTML buffer (filename or '-' maps to HTML "
         "editor)\n"
         "  -c, --compact          Emit compact single-line JSON\n"
         "  -h, --help             Print this help message\n";
}

} // namespace

/// Executes a DSL string against HTML and returns a JSON status envelope.
std::string run_dsl(const std::string &html, const std::string &dsl,
                    const std::string &scraper_name, bool compact) {
  const auto start = std::chrono::steady_clock::now();
  try {
    scraper::dsl::DslParser parser;
    auto scrapers = parser.parse(dsl);
    const jsoncons::json out =
        xsel::cli::run_dsl_scrapers(scrapers, scraper_name, html);
    return make_envelope(true, format_json(out, compact), "",
                         elapsed_since(start));
  } catch (const std::exception &e) {
    return make_envelope(false, "", e.what(), elapsed_since(start));
  }
}

/// Executes a quick CSS selector extraction against HTML.
std::string run_selector(const std::string &html, const std::string &selector,
                         bool compact) {
  const auto start = std::chrono::steady_clock::now();
  try {
    if (selector.empty()) {
      throw std::runtime_error("CSS selector cannot be empty.");
    }
    scraper::ScraperBuilder builder;
    builder.Select(selector, "matches").Text().Squish().Set("text");
    const jsoncons::json out = builder.scrape_from_string(html);
    return make_envelope(true, format_json(out, compact), "",
                         elapsed_since(start));
  } catch (const std::exception &e) {
    return make_envelope(false, "", e.what(), elapsed_since(start));
  }
}

/// Executes an `xsel2` CLI command string against in-memory HTML and DSL.
std::string run_cli(const std::string &command_line, const std::string &html,
                    const std::string &dsl) {
  const auto start = std::chrono::steady_clock::now();
  try {
    const std::vector<std::string> tokens = split_command_line(command_line);
    if (tokens.empty()) {
      return make_cli_envelope(false, "", cli_usage_text(), 1,
                               elapsed_since(start));
    }

    size_t idx = (tokens[0] == "xsel2" || tokens[0] == "./build/xsel2") ? 1 : 0;
    if (idx >= tokens.size()) {
      return make_cli_envelope(false, "", cli_usage_text(), 1,
                               elapsed_since(start));
    }

    std::string dsl_arg;
    std::string scraper_name;
    std::string selector;
    std::string html_arg;
    bool has_html = false;
    bool compact = false;

    auto next_val = [&](std::string_view flag) -> std::string {
      if (idx + 1 >= tokens.size()) {
        throw std::runtime_error("Missing value for flag: " +
                                 std::string(flag));
      }
      return tokens[++idx];
    };

    for (; idx < tokens.size(); ++idx) {
      const std::string &arg = tokens[idx];
      if (arg == "-h" || arg == "--help") {
        return make_cli_envelope(true, cli_usage_text(), "", 0,
                                 elapsed_since(start));
      } else if (arg == "-d" || arg == "--dsl") {
        dsl_arg = next_val(arg);
      } else if (arg == "-n" || arg == "--name") {
        scraper_name = next_val(arg);
      } else if (arg == "-s" || arg == "--selector") {
        selector = next_val(arg);
      } else if (arg == "--html") {
        html_arg = next_val(arg);
        has_html = true;
      } else if (arg == "-c" || arg == "--compact") {
        compact = true;
      } else if (arg == "--batch" || arg == "--ndjson" ||
                 arg == "--json-array" || arg == "--json-object" ||
                 arg == "--out-dir" || arg == "-j" || arg == "--threads") {
        throw std::runtime_error(
            "Batch/threading flags are not supported in the browser WASM "
            "sandbox: " +
            arg);
      } else {
        throw std::runtime_error("Unknown flag: " + arg);
      }
    }

    if (!dsl_arg.empty() && !selector.empty()) {
      throw std::runtime_error("--dsl and --selector are mutually exclusive.");
    }
    if (dsl_arg.empty() && selector.empty()) {
      throw std::runtime_error("--dsl (-d) or --selector (-s) is required.");
    }

    // Map inline HTML (if it looks like markup) or fall back to HTML editor.
    const std::string &effective_html =
        (has_html && html_arg != "-" &&
         html_arg.find('<') != std::string::npos)
            ? html_arg
            : html;

    if (!selector.empty()) {
      scraper::ScraperBuilder builder;
      builder.Select(selector, "matches").Text().Squish().Set("text");
      const jsoncons::json out = builder.scrape_from_string(effective_html);
      return make_cli_envelope(true, format_json(out, compact), "", 0,
                               elapsed_since(start));
    }

    scraper::dsl::DslParser parser;
    auto scrapers = parser.parse(dsl);
    const jsoncons::json out =
        xsel::cli::run_dsl_scrapers(scrapers, scraper_name, effective_html);
    return make_cli_envelope(true, format_json(out, compact), "", 0,
                             elapsed_since(start));
  } catch (const std::exception &e) {
    return make_cli_envelope(false, "", std::string("Error: ") + e.what(), 1,
                             elapsed_since(start));
  }
}

EMSCRIPTEN_BINDINGS(xsel2_wasm_module) {
  emscripten::function("run_dsl", &run_dsl);
  emscripten::function("run_selector", &run_selector);
  emscripten::function("run_cli", &run_cli);
}
