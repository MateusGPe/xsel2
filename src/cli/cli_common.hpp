#pragma once

#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <absl/container/flat_hash_map.h>
#include <iostream>
#include <jsoncons/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xsel::cli {

inline void print_json(const jsoncons::json &doc, bool compact) {
  if (compact) {
    std::string out;
    doc.dump(out);
    std::cout << out << '\n';
  } else {
    std::cout << jsoncons::pretty_print(doc) << '\n';
  }
}

[[nodiscard]] inline jsoncons::json run_dsl_scrapers(
    absl::flat_hash_map<std::string, scraper::ScraperBuilder> &scrapers,
    std::string_view scraper_name, std::string_view html) {
  if (scrapers.empty()) {
    throw std::runtime_error("No scrapers found in DSL file.");
  }

  if (!scraper_name.empty()) {
    auto it = scrapers.find(scraper_name);
    if (it == scrapers.end()) {
      throw std::runtime_error("Scraper '" + std::string(scraper_name) +
                               "' not found.");
    }
    return it->second.scrape_from_string(html);
  }

  if (scrapers.size() == 1) {
    return scrapers.begin()->second.scrape_from_string(html);
  }

  Document doc;
  doc.parseMemory(html);
  jsoncons::json all = jsoncons::json::object();
  for (auto &[name, builder] : scrapers) {
    jsoncons::json out = jsoncons::json::object();
    builder.scrape_document(doc, out, name);
    all.insert_or_assign(name, std::move(out));
  }
  return all;
}

} // namespace xsel::cli
