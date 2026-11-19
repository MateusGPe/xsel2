#include "common/ahocorasick.hpp"
#include "query/json_query.hpp"
#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <fstream>
#include <iostream>
#include <jsoncons/json.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string read_file_or_stdin(std::string_view path) {
  if (path == "-") {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
  }
  std::ifstream ifs{std::string(path)};
  if (!ifs.is_open()) {
    throw std::runtime_error("Cannot open file: " + std::string(path));
  }
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::string read_arg_or_file(std::string_view value) {
  if (value == "-") {
    return read_file_or_stdin("-");
  }
  std::ifstream probe{std::string(value)};
  if (probe.is_open()) {
    std::ostringstream ss;
    ss << probe.rdbuf();
    return ss.str();
  }
  return std::string(value);
}

void print_usage() {
  std::cout << "xmlsel-cli - High-Performance Scraping & JSON Query CLI\n\n"
            << "Usage:\n"
            << "  xmlsel-cli scraper (--dsl <file> [--name <scraper>] | "
               "--selector <css>) --html <file|str|-> [--compact]\n"
            << "  xmlsel-cli ahocorasick --patterns <p1,p2,...> --text "
               "<file|str|->\n"
            << "  xmlsel-cli jmespath --expr <expr> --json <file|str|-> "
               "[--compact]\n"
            << "  xmlsel-cli jsonpath --expr <expr> --json <file|str|-> "
               "[--compact]\n";
}

void print_json(const jsoncons::json &doc, bool compact) {
  if (compact) {
    std::string out;
    doc.dump(out);
    std::cout << out << '\n';
  } else {
    std::cout << jsoncons::pretty_print(doc) << '\n';
  }
}

int cmd_scraper(const std::vector<std::string_view> &args) {
  std::string dsl_path;
  std::string scraper_name;
  std::string selector;
  std::string html_input;
  bool compact = false;

  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--dsl" && i + 1 < args.size()) {
      dsl_path = std::string(args[++i]);
    } else if (args[i] == "--name" && i + 1 < args.size()) {
      scraper_name = std::string(args[++i]);
    } else if (args[i] == "--selector" && i + 1 < args.size()) {
      selector = std::string(args[++i]);
    } else if (args[i] == "--html" && i + 1 < args.size()) {
      html_input = std::string(args[++i]);
    } else if (args[i] == "--compact" || args[i] == "-c") {
      compact = true;
    }
  }

  if (html_input.empty()) {
    std::cerr << "Error: --html is required.\n";
    return 1;
  }
  const std::string html = read_arg_or_file(html_input);

  if (!selector.empty()) {
    scraper::ScraperBuilder builder;
    builder.Group(selector, "matches").Text().Normspace().Set("text");
    print_json(builder.scrape_from_string(html), compact);
    return 0;
  }

  if (dsl_path.empty()) {
    std::cerr << "Error: --dsl or --selector is required.\n";
    return 1;
  }

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse_file(dsl_path);
  if (scrapers.empty()) {
    std::cerr << "Error: No scrapers found in DSL file.\n";
    return 1;
  }

  if (!scraper_name.empty()) {
    auto it = scrapers.find(scraper_name);
    if (it == scrapers.end()) {
      std::cerr << "Error: Scraper '" << scraper_name << "' not found.\n";
      return 1;
    }
    print_json(it->second.scrape_from_string(html), compact);
    return 0;
  }

  if (scrapers.size() == 1) {
    print_json(scrapers.begin()->second.scrape_from_string(html), compact);
    return 0;
  }

  jsoncons::json all = jsoncons::json::object();
  for (auto &[name, builder] : scrapers) {
    all.insert_or_assign(name, builder.scrape_from_string(html));
  }
  print_json(all, compact);
  return 0;
}

int cmd_ahocorasick(const std::vector<std::string_view> &args) {
  std::string patterns_arg;
  std::string text_arg;
  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--patterns" && i + 1 < args.size()) {
      patterns_arg = std::string(args[++i]);
    } else if (args[i] == "--text" && i + 1 < args.size()) {
      text_arg = std::string(args[++i]);
    }
  }
  if (patterns_arg.empty() || text_arg.empty()) {
    std::cerr << "Error: --patterns and --text are required.\n";
    return 1;
  }

  AhoCorasick ac;
  size_t pos = 0;
  while (pos <= patterns_arg.size()) {
    const size_t next = patterns_arg.find(',', pos);
    const size_t end = (next == std::string::npos) ? patterns_arg.size() : next;
    std::string pat = patterns_arg.substr(pos, end - pos);
    if (!pat.empty())
      ac.AddPattern(pat);
    if (next == std::string::npos)
      break;
    pos = next + 1;
  }
  ac.Compile();

  const std::string text = read_arg_or_file(text_arg);
  AhoCorasick::MatchList raw_matches;
  ac.Find(text, raw_matches);
  jsoncons::json matches = jsoncons::json::array();
  for (const auto &match : raw_matches) {
    jsoncons::json m = jsoncons::json::object();
    m.insert_or_assign("start", static_cast<uint64_t>(match.start));
    m.insert_or_assign("end",
                       static_cast<uint64_t>(match.start + match.length));
    m.insert_or_assign("match", text.substr(match.start, match.length));
    m.insert_or_assign("pattern_id", static_cast<uint64_t>(match.pattern_id));
    matches.push_back(std::move(m));
  }
  print_json(matches, false);
  return 0;
}

int cmd_query(xsel::query::QueryDialect dialect,
              const std::vector<std::string_view> &args) {
  std::string expr;
  std::string json_arg;
  bool compact = false;

  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--expr" && i + 1 < args.size()) {
      expr = std::string(args[++i]);
    } else if (args[i] == "--json" && i + 1 < args.size()) {
      json_arg = std::string(args[++i]);
    } else if (args[i] == "--compact" || args[i] == "-c") {
      compact = true;
    }
  }
  if (expr.empty() || json_arg.empty()) {
    std::cerr << "Error: --expr and --json are required.\n";
    return 1;
  }

  const std::string raw = read_arg_or_file(json_arg);
  const jsoncons::json doc = jsoncons::json::parse(raw);
  const jsoncons::json result = xsel::query::evaluate(dialect, doc, expr);
  print_json(result, compact);
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  std::vector<std::string_view> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
    print_usage();
    return 0;
  }

  try {
    if (args[1] == "scraper") {
      return cmd_scraper(args);
    }
    if (args[1] == "ahocorasick") {
      return cmd_ahocorasick(args);
    }
    if (args[1] == "jmespath") {
      return cmd_query(xsel::query::QueryDialect::JmesPath, args);
    }
    if (args[1] == "jsonpath") {
      return cmd_query(xsel::query::QueryDialect::JsonPath, args);
    }
    std::cerr << "Unknown subcommand: " << args[1] << '\n';
    print_usage();
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
