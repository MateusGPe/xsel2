#pragma once

#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <iostream>
#include <lxml2-query.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

enum class ErrorPolicy {
  LogAndContinue, ///< Format error to error_stream, clear values, continue
  ThrowOnError,   ///< Rethrow step exceptions immediately
  CollectErrors   ///< Append formatted error to errors vector, clear values, continue
};

struct ExtractionOptions {
  ErrorPolicy error_policy = ErrorPolicy::LogAndContinue;
  std::vector<std::string> *errors = nullptr;
};

class Extractor {
public:
  Extractor() = default;
  Extractor(SelectorPtr selector, std::string_view name)
      : selector_(std::move(selector)),
        name_(name),
        name_segments_(name_.empty() ? std::vector<std::string>{}
                                     : split_dict_path(name_, true)) {}

  void append_step(std::shared_ptr<Step> step);

  void extract(QueryResult &selection, jsoncons::json &dest,
               std::string_view err_prefix, std::ostream &stream = std::cerr);

  void extract(QueryResult &selection, jsoncons::json &dest,
               std::string_view err_prefix, std::ostream &stream,
               const ExtractionOptions &opts);

private:
  std::vector<std::shared_ptr<Step>> steps_;
  SelectorPtr selector_;
  std::string name_;
  std::vector<std::string> name_segments_;
};

} // namespace scraper
