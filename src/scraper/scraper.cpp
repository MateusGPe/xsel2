#include "scraper/scraper.hpp"
#include "scraper/merge.hpp"
#include <iostream>
#include <stdexcept>

namespace scraper {

Scraper::Scraper(const Scraper &other)
    : finalize_steps_(other.finalize_steps_), error_policy_(other.error_policy_),
      errors_(other.errors_), in_block_(other.in_block_) {
  extractors_.reserve(other.extractors_.size());
  for (const auto &ext : other.extractors_) {
    extractors_.push_back(ext ? std::make_shared<Extractor>(*ext) : nullptr);
  }
}

Scraper &Scraper::operator=(const Scraper &other) {
  if (this != &other) {
    std::vector<std::shared_ptr<Extractor>> cloned;
    cloned.reserve(other.extractors_.size());
    for (const auto &ext : other.extractors_) {
      cloned.push_back(ext ? std::make_shared<Extractor>(*ext) : nullptr);
    }
    extractors_ = std::move(cloned);
    finalize_steps_ = other.finalize_steps_;
    error_policy_ = other.error_policy_;
    errors_ = other.errors_;
    in_block_ = other.in_block_;
  }
  return *this;
}

void Scraper::add_step(std::shared_ptr<Step> step) {
  if (extractors_.empty()) {
    extractors_.push_back(std::make_shared<Extractor>(nullptr, ""));
  }
  extractors_.back()->append_step(std::move(step));
}

void Scraper::add_dict_step(std::shared_ptr<Step> step) {
  if (!step) {
    return;
  }
  if (in_block_) {
    add_step(std::move(step));
  } else {
    finalize_steps_.push_back(std::move(step));
  }
}

void Scraper::scrape_document(Document &document, jsoncons::json &data,
                              std::string_view error_prefix,
                              std::ostream &error_stream) {
  ExtractionOptions opts{error_policy_, &errors_};
  scrape_document(document, data, error_prefix, error_stream, opts);
}

void Scraper::scrape_document(Document &document, jsoncons::json &data,
                              std::string_view error_prefix,
                              std::ostream &error_stream,
                              const ExtractionOptions &opts) {
  QueryResult root(document.getRoot());
  perform_extraction(root, data, error_prefix, error_stream, opts);
}

void Scraper::perform_extraction(QueryResult &selection, jsoncons::json &data,
                                 std::string_view error_prefix,
                                 std::ostream &error_stream) {
  ExtractionOptions opts{error_policy_, &errors_};
  perform_extraction(selection, data, error_prefix, error_stream, opts);
}

void Scraper::perform_extraction(QueryResult &selection, jsoncons::json &data,
                                 std::string_view error_prefix,
                                 std::ostream &error_stream,
                                 const ExtractionOptions &opts) {
  if (opts.errors == &errors_) {
    errors_.clear();
  }
  bool aborted = false;
  ExtractionOptions local_opts = opts;
  local_opts.aborted = &aborted;
  for (auto &ext : extractors_) {
    ext->extract(selection, data, error_prefix, error_stream, local_opts);
    if (aborted) {
      break;
    }
  }
  preprocess_key_value(data);

  if (!finalize_steps_.empty()) {
    Node root_node = selection.front().value_or(Node(nullptr));
    ExecutionContext ctx;
    std::vector<std::string> empty_vals;
    for (const auto &step : finalize_steps_) {
      if (!step) {
        continue;
      }
      try {
        step->apply(root_node, empty_vals, data, ctx);
      } catch (const std::exception &e) {
        if (local_opts.error_policy == ErrorPolicy::ThrowOnError) {
          throw;
        }
        std::string msg;
        if (!error_prefix.empty()) {
          msg = "[" + std::string(error_prefix) + "] ";
        }
        msg += "Extractor error (finalize): " + std::string(e.what());
        if (local_opts.error_policy == ErrorPolicy::CollectErrors) {
          if (local_opts.errors != nullptr) {
            local_opts.errors->push_back(std::move(msg));
          }
        } else {
          error_stream << msg << '\n';
        }
        empty_vals.clear();
      }
      if (ctx.abort_scraper) {
        break;
      }
    }
    preprocess_key_value(data);
  }
}

jsoncons::json Scraper::scrape_from_string(std::string_view content) {
  Document doc;
  doc.parseMemory(content);
  jsoncons::json result = jsoncons::json::object();
  scrape_document(doc, result, "string_content");
  return result;
}

jsoncons::json Scraper::scrape_from_file(std::string_view filename) {
  Document doc;
  if (!doc.parse(filename)) {
    std::cerr << "[xsel2] Warning: Failed to open or parse file: " << filename
              << '\n';
    return jsoncons::json::object();
  }
  jsoncons::json result = jsoncons::json::object();
  scrape_document(doc, result, filename);
  return result;
}

} // namespace scraper
