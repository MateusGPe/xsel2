#include "cli/cli_common.hpp"
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

  try {
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg(argv[i]);
      if (arg == "-d" || arg == "--dsl") {
        if (i + 1 >= argc) {
          throw std::runtime_error("Missing value for " + std::string(arg));
        }
        dsl_path = argv[++i];
      } else if (arg == "-n" || arg == "--name") {
        if (i + 1 >= argc) {
          throw std::runtime_error("Missing value for " + std::string(arg));
        }
        scraper_name = argv[++i];
      } else if (arg == "-c" || arg == "--compact") {
        compact = true;
      } else if (arg == "-h" || arg == "--help") {
        std::cout
            << "Usage: xmlsel-pipe -d <dsl_file> [-n <scraper_name>] [-c]\n"
            << "Reads HTML from stdin and writes JSON to stdout.\n";
        return 0;
      } else {
        throw std::runtime_error("Unknown flag: " + std::string(arg));
      }
    }

    if (dsl_path.empty()) {
      std::cerr << "Error: -d <dsl_file> is required.\n";
      return 1;
    }
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    const std::string html = ss.str();

    scraper::dsl::DslParser parser;
    auto scrapers = parser.parse_file(dsl_path);
    const jsoncons::json out =
        xsel::cli::run_dsl_scrapers(scrapers, scraper_name, html);
    xsel::cli::print_json(out, compact);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
