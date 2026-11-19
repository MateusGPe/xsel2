#include "scraper/dsl_parser.hpp"
#include <iostream>
#include <jsoncons/json.hpp>
#include <sstream>
#include <string>
#include <string_view>

int main(int argc, char *argv[]) {
  std::string dsl_path;
  std::string scraper_name;
  bool compact = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if ((arg == "-d" || arg == "--dsl") && i + 1 < argc) {
      dsl_path = argv[++i];
    } else if ((arg == "-n" || arg == "--name") && i + 1 < argc) {
      scraper_name = argv[++i];
    } else if (arg == "-c" || arg == "--compact") {
      compact = true;
    } else if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: xmlsel-pipe -d <dsl_file> [-n <scraper_name>] [-c]\n"
                << "Reads HTML from stdin and writes JSON to stdout.\n";
      return 0;
    }
  }

  if (dsl_path.empty()) {
    std::cerr << "Error: -d <dsl_file> is required.\n";
    return 1;
  }

  try {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    const std::string html = ss.str();

    scraper::dsl::DslParser parser;
    auto scrapers = parser.parse_file(dsl_path);
    if (scrapers.empty()) {
      std::cerr << "Error: No scrapers defined in " << dsl_path << '\n';
      return 1;
    }

    jsoncons::json out = jsoncons::json::object();
    if (!scraper_name.empty()) {
      auto it = scrapers.find(scraper_name);
      if (it == scrapers.end()) {
        std::cerr << "Error: Scraper '" << scraper_name << "' not found.\n";
        return 1;
      }
      out = it->second.scrape_from_string(html);
    } else if (scrapers.size() == 1) {
      out = scrapers.begin()->second.scrape_from_string(html);
    } else {
      for (auto &[name, builder] : scrapers) {
        out.insert_or_assign(name, builder.scrape_from_string(html));
      }
    }

    if (compact) {
      std::string raw;
      out.dump(raw);
      std::cout << raw << '\n';
    } else {
      std::cout << jsoncons::pretty_print(out) << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
