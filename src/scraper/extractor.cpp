#include "scraper/extractor.hpp"
#include "common/json_utils.hpp"
#include "scraper/merge.hpp"
#include <exception>
#include <iterator>
#include <utility>

namespace scraper {

void Extractor::append_step(std::shared_ptr<Step> step) {
  steps_.push_back(std::move(step));
}

void Extractor::extract(QueryResult &selection, jsoncons::json &dest,
                        std::string_view err_prefix, std::ostream &stream) {
  QueryResult sel = selection;

  if (selector_ != nullptr) {
    sel = sel.Find(selector_);
    if (sel.size() == 0) {
      return;
    }
  }

  auto log_error = [&](const std::exception &e) {
    if (!err_prefix.empty()) {
      stream << '[' << err_prefix << "] ";
    }
    stream << "Extractor error";
    if (!name_.empty()) {
      stream << " (" << name_ << ")";
    }
    stream << ": " << e.what() << '\n';
  };

  std::shared_ptr<Step> postponed_step = nullptr;

  for (const auto &sel_node : sel) {
    jsoncons::json item = jsoncons::json::null();
    std::vector<std::string> values;

    for (auto it = steps_.begin(); it != steps_.end(); ++it) {
      auto &step = *it;
      try {
        if (step->is_postponable()) {
          if (std::next(it) == steps_.end()) {
            postponed_step = step;
          } else {
            dest = step->execute_postponed(dest);
          }
          continue;
        }
        step->apply(sel_node, values, item);
      } catch (const std::exception &e) {
        log_error(e);
        values.clear();
      }
    }

    const bool has_data = !xsel::common::is_empty_json(item);
    if (!has_data) {
      continue;
    }

    if (dest.is_object() || dest.is_null()) {
      if (name_segments_.empty()) {
        merge_into(dest, std::move(item));
      } else {
        if (dest.is_null()) {
          dest = jsoncons::json::object();
        }
        jsoncons::json *target = &dest;
        for (size_t i = 0; i + 1 < name_segments_.size(); ++i) {
          if (!target->contains(name_segments_[i]) ||
              !target->at(name_segments_[i]).is_object()) {
            target->insert_or_assign(name_segments_[i],
                                     jsoncons::json::object());
          }
          target = &target->at(name_segments_[i]);
        }
        if (!target->contains(name_segments_.back())) {
          target->insert_or_assign(name_segments_.back(),
                                   jsoncons::json::null());
        }
        merge_into(target->at(name_segments_.back()), std::move(item));
      }
    } else if (dest.is_array()) {
      if (name_segments_.empty()) {
        merge_into(dest, std::move(item));
      } else {
        preprocess_key_value(item);
        if (!xsel::common::is_empty_json(item)) {
          jsoncons::json wrapped = std::move(item);
          for (size_t i = name_segments_.size(); i > 0; --i) {
            jsoncons::json obj = jsoncons::json::object();
            obj.insert_or_assign(name_segments_[i - 1], std::move(wrapped));
            wrapped = std::move(obj);
          }
          merge_into(dest, std::move(wrapped));
        }
      }
    }
  }

  if (postponed_step) {
    try {
      dest = postponed_step->execute_postponed(dest);
    } catch (const std::exception &e) {
      log_error(e);
    }
  }
}

} // namespace scraper
