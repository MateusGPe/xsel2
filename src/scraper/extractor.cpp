#include "scraper/extractor.hpp"
#include "scraper/merge.hpp"
#include <exception>
#include <sstream>
#include <utility>

namespace scraper {

namespace {

std::string format_extractor_error(std::string_view err_prefix,
                                   std::string_view name,
                                   const std::exception &e) {
  std::ostringstream oss;
  if (!err_prefix.empty()) {
    oss << '[' << err_prefix << "] ";
  }
  oss << "Extractor error";
  if (!name.empty()) {
    oss << " (" << name << ')';
  }
  oss << ": " << e.what();
  return oss.str();
}

void handle_step_error(const std::exception &e, std::string_view err_prefix,
                       std::string_view name, std::ostream &stream,
                       const ExtractionOptions &opts) {
  switch (opts.error_policy) {
  case ErrorPolicy::ThrowOnError:
    throw;
  case ErrorPolicy::CollectErrors:
    if (opts.errors != nullptr) {
      opts.errors->push_back(format_extractor_error(err_prefix, name, e));
    }
    break;
  case ErrorPolicy::LogAndContinue:
    stream << format_extractor_error(err_prefix, name, e) << '\n';
    break;
  }
}

} // namespace

void Extractor::append_step(std::shared_ptr<Step> step) {
  if (!step) {
    return;
  }
  steps_.push_back(std::move(step));
}

void Extractor::extract(QueryResult &selection, jsoncons::json &dest,
                        std::string_view err_prefix, std::ostream &stream) {
  extract(selection, dest, err_prefix, stream, ExtractionOptions{});
}

void Extractor::extract(QueryResult &selection, jsoncons::json &dest,
                        std::string_view err_prefix, std::ostream &stream,
                        const ExtractionOptions &opts) {
  QueryResult sel = selection;

  if (selector_ != nullptr) {
    sel = sel.Find(selector_);
    if (sel.size() == 0) {
      return;
    }
  }

  std::vector<std::shared_ptr<Step>> postponed_steps;
  for (const auto &step : steps_) {
    if (step->is_postponable()) {
      postponed_steps.push_back(step);
    }
  }

  for (const auto &sel_node : sel) {
    jsoncons::json item = jsoncons::json::null();
    std::vector<std::string> values;
    ExecutionContext ctx;

    for (const auto &step : steps_) {
      if (step->is_postponable()) {
        continue;
      }
      try {
        step->apply(sel_node, values, item, ctx);
      } catch (const std::exception &e) {
        handle_step_error(e, err_prefix, name_, stream, opts);
        values.clear();
      }
      if (ctx.abort_scraper) {
        if (opts.aborted != nullptr) {
          *opts.aborted = true;
        }
        break;
      }
      if (ctx.skip_item) {
        break;
      }
    }

    if (!ctx.skip_item) {
      merge_item_at_path(dest, name_segments_, std::move(item));
    }
    if (ctx.abort_scraper) {
      return;
    }
  }

  for (const auto &postponed : postponed_steps) {
    try {
      dest = postponed->execute_postponed(dest);
    } catch (const std::exception &e) {
      handle_step_error(e, err_prefix, name_, stream, opts);
    }
  }
}

} // namespace scraper
