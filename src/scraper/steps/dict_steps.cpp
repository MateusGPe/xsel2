#include "scraper/steps/dict_steps.hpp"
#include "common/json_utils.hpp"
#include <algorithm>

namespace scraper {

namespace {

void add_or_append_json(jsoncons::json &obj, const std::string &key,
                        const jsoncons::json &val) {
  if (!obj.contains(key)) {
    obj.insert_or_assign(key, val);
    return;
  }
  jsoncons::json &existing = obj.at(key);
  if (existing.is_array()) {
    existing.push_back(val);
  } else {
    jsoncons::json arr = jsoncons::json::array();
    arr.push_back(std::move(existing));
    arr.push_back(val);
    existing = std::move(arr);
  }
}

void flatten_recursive(const jsoncons::json &cur, jsoncons::json &out,
                       const std::string &prefix, int depth) {
  xsel::common::check_json_depth(depth, "FlattenDictStep");

  if (cur.is_object()) {
    for (const auto &kv : cur.object_range()) {
      const std::string key =
          prefix.empty() ? std::string(kv.key())
                         : prefix + "_" + std::string(kv.key());
      flatten_recursive(kv.value(), out, key, depth + 1);
    }
    return;
  }

  if (cur.is_array()) {
    for (const auto &el : cur.array_range()) {
      flatten_recursive(el, out, prefix, depth + 1);
    }
    return;
  }

  if (!cur.is_null() && !prefix.empty()) {
    add_or_append_json(out, prefix, cur);
  }
}

jsoncons::json filter_recursive(
    const jsoncons::json &cur,
    const absl::flat_hash_set<std::string> &keys_to_keep, int depth) {
  xsel::common::check_json_depth(depth, "FilterDictStep");

  if (cur.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &kv : cur.object_range()) {
      if (keys_to_keep.contains(kv.key())) {
        out.insert_or_assign(kv.key(), kv.value());
      } else if (kv.value().is_object() || kv.value().is_array()) {
        jsoncons::json child =
            filter_recursive(kv.value(), keys_to_keep, depth + 1);
        if (!xsel::common::is_empty_json(child)) {
          out.insert_or_assign(kv.key(), std::move(child));
        }
      }
    }
    return out;
  }

  if (cur.is_array()) {
    jsoncons::json out = jsoncons::json::array();
    for (const auto &el : cur.array_range()) {
      jsoncons::json child = filter_recursive(el, keys_to_keep, depth + 1);
      if (!xsel::common::is_empty_json(child)) {
        out.push_back(std::move(child));
      }
    }
    return out;
  }

  return jsoncons::json::null();
}

} // namespace

// ── FlattenDictStep ──────────────────────────────────────────────────────────

jsoncons::json
FlattenDictStep::transform(const jsoncons::json &input) const {
  return flatten_impl(input, 0);
}

jsoncons::json FlattenDictStep::flatten_impl(const jsoncons::json &input,
                                             int depth) const {
  xsel::common::check_json_depth(depth, "FlattenDictStep");
  if (input.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    flatten_recursive(input, out, "", depth);
    return out;
  }
  if (input.is_array()) {
    jsoncons::json out = jsoncons::json::array();
    for (const auto &el : input.array_range()) {
      out.push_back(flatten_impl(el, depth + 1));
    }
    return out;
  }
  return input;
}

// ── FilterDictStep ───────────────────────────────────────────────────────────

jsoncons::json
FilterDictStep::transform(const jsoncons::json &input) const {
  return filter_recursive(input, keys_to_keep_, 0);
}

// ── RemoveKeysStep ───────────────────────────────────────────────────────────

jsoncons::json
RemoveKeysStep::transform(const jsoncons::json &input) const {
  return remove_impl(input, 0);
}

jsoncons::json RemoveKeysStep::remove_impl(const jsoncons::json &input,
                                           int depth) const {
  xsel::common::check_json_depth(depth, "RemoveKeysStep");
  if (input.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &kv : input.object_range()) {
      if (!keys_to_remove_.contains(kv.key())) {
        out.insert_or_assign(kv.key(), remove_impl(kv.value(), depth + 1));
      }
    }
    return out;
  }
  if (input.is_array()) {
    jsoncons::json out = jsoncons::json::array();
    for (const auto &el : input.array_range()) {
      out.push_back(remove_impl(el, depth + 1));
    }
    return out;
  }
  return input;
}

// ── RenameKeysStep ───────────────────────────────────────────────────────────

jsoncons::json
RenameKeysStep::transform(const jsoncons::json &input) const {
  return rename_impl(input, 0);
}

jsoncons::json RenameKeysStep::rename_impl(const jsoncons::json &input,
                                           int depth) const {
  xsel::common::check_json_depth(depth, "RenameKeysStep");
  if (input.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &kv : input.object_range()) {
      const auto it = rename_map_.find(kv.key());
      const std::string_view new_key =
          (it != rename_map_.end()) ? std::string_view(it->second) : kv.key();
      out.insert_or_assign(new_key, rename_impl(kv.value(), depth + 1));
    }
    return out;
  }
  if (input.is_array()) {
    jsoncons::json out = jsoncons::json::array();
    for (const auto &el : input.array_range()) {
      out.push_back(rename_impl(el, depth + 1));
    }
    return out;
  }
  return input;
}

// ── MergeKeysStep ────────────────────────────────────────────────────────────

jsoncons::json
MergeKeysStep::transform(const jsoncons::json &input) const {
  if (!input.is_object()) {
    return input;
  }
  jsoncons::json out = input;
  jsoncons::json merged_arr = jsoncons::json::array();
  for (const auto &k : source_keys_) {
    if (!out.contains(k)) {
      continue;
    }
    jsoncons::json &val = out.at(k);
    if (val.is_array()) {
      for (auto &el : val.array_range()) {
        merged_arr.push_back(std::move(el));
      }
    } else if (!val.is_null()) {
      merged_arr.push_back(std::move(val));
    }
    out.erase(k);
  }
  if (!merged_arr.empty()) {
    out.insert_or_assign(target_key_, std::move(merged_arr));
  }
  return out;
}

// ── SortKeysStep ─────────────────────────────────────────────────────────────

jsoncons::json
SortKeysStep::transform(const jsoncons::json &input) const {
  return sort_impl(input, 0);
}

jsoncons::json SortKeysStep::sort_impl(const jsoncons::json &input,
                                       int depth) const {
  xsel::common::check_json_depth(depth, "SortKeysStep");
  if (input.is_array()) {
    std::vector<jsoncons::json> items;
    items.reserve(input.size());
    for (const auto &el : input.array_range()) {
      items.push_back(sort_impl(el, depth + 1));
    }
    std::sort(items.begin(), items.end(),
              [this](const jsoncons::json &a, const jsoncons::json &b) {
                const int cmp = xsel::common::compare_json(a, b);
                return ascending_ ? (cmp < 0) : (cmp > 0);
              });
    jsoncons::json out = jsoncons::json::array();
    for (auto &item : items) {
      out.push_back(std::move(item));
    }
    return out;
  }
  if (input.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &kv : input.object_range()) {
      out.insert_or_assign(kv.key(), sort_impl(kv.value(), depth + 1));
    }
    return out;
  }
  return input;
}

// ── DeduplicateStep ──────────────────────────────────────────────────────────

jsoncons::json
DeduplicateStep::transform(const jsoncons::json &input) const {
  return dedup_impl(input, 0);
}

jsoncons::json DeduplicateStep::dedup_impl(const jsoncons::json &input,
                                           int depth) const {
  xsel::common::check_json_depth(depth, "DeduplicateStep");
  if (input.is_array()) {
    jsoncons::json out = jsoncons::json::array();
    absl::flat_hash_set<std::string> seen;
    seen.reserve(input.size());
    for (const auto &el : input.array_range()) {
      jsoncons::json item = dedup_impl(el, depth + 1);
      std::string key = xsel::common::canonical_json_key(item);
      if (seen.insert(std::move(key)).second) {
        out.push_back(std::move(item));
      }
    }
    return out;
  }
  if (input.is_object()) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &kv : input.object_range()) {
      out.insert_or_assign(kv.key(), dedup_impl(kv.value(), depth + 1));
    }
    return out;
  }
  return input;
}

// ── JsonQueryStep ────────────────────────────────────────────────────────────

jsoncons::json
JsonQueryStep::transform(const jsoncons::json &input) const {
  return xsel::query::evaluate(dialect_, input, expr_);
}

} // namespace scraper
