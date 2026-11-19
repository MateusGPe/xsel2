#include "scraper/scraper.hpp"
#include "scraper/merge.hpp"
#include <stdexcept>

namespace scraper {

void Scraper::add_step(std::shared_ptr<Step> step) {
  if (extractors_.empty()) {
    extractors_.push_back(std::make_shared<Extractor>(nullptr, ""));
  }
  extractors_.back()->append_step(std::move(step));
}

void Scraper::scrape_document(Document &document, jsoncons::json &data,
                              std::string_view error_prefix,
                              std::ostream &error_stream) {
  QueryResult root(document.getRoot());
  perform_extraction(root, data, error_prefix, error_stream);
}

void Scraper::perform_extraction(QueryResult &selection, jsoncons::json &data,
                                 std::string_view error_prefix,
                                 std::ostream &error_stream) {
  for (auto &ext : extractors_) {
    ext->extract(selection, data, error_prefix, error_stream);
  }
  preprocess_key_value(data);
}

jsoncons::json Scraper::scrape_from_string(std::string_view content) {
  Document doc;
  doc.parseMemory(std::string(content));
  jsoncons::json result = jsoncons::json::object();
  scrape_document(doc, result, "string_content");
  return result;
}

jsoncons::json Scraper::scrape_from_file(std::string_view filename) {
  Document doc;
  doc.parse(std::string(filename));
  jsoncons::json result = jsoncons::json::object();
  scrape_document(doc, result, filename);
  return result;
}

} // namespace scraper
