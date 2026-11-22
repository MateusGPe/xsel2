#include "scraper/steps/dict_steps.hpp"
#include "common/json_utils.hpp"
#include "scraper/merge.hpp"
#include <algorithm>
#include <stdexcept>

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

// ── SortArrayStep ────────────────────────────────────────────────────────────

jsoncons::json
SortArrayStep::transform(const jsoncons::json &input) const {
  return sort_impl(input, 0);
}

jsoncons::json SortArrayStep::sort_impl(const jsoncons::json &input,
                                        int depth) const {
  xsel::common::check_json_depth(depth, "SortArrayStep");
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

jsoncons::json ZipKeysStep::transform(const jsoncons::json &input) const {
  if (!input.is_object() || !input.contains(keys_key_) ||
      !input.contains(values_key_)) {
    return input;
  }
  const auto &keys = input[keys_key_];
  const auto &vals = input[values_key_];
  if (!keys.is_array() || !vals.is_array()) {
    return input;
  }

  jsoncons::json zipped = jsoncons::json::object();
  const size_t n = std::min(keys.size(), vals.size());
  for (size_t i = 0; i < n; ++i) {
    zipped.insert_or_assign(xsel::common::json_to_string(keys[i]), vals[i]);
  }

  jsoncons::json out = input;
  if (target_key_.empty()) {
    for (const auto &kv : zipped.object_range()) {
      out.insert_or_assign(kv.key(), kv.value());
    }
  } else {
    out.insert_or_assign(target_key_, std::move(zipped));
  }
  return out;
}

// ── TeeStep / SaveValuesStep / RestoreValuesStep / CoalesceKeysStep ──────────

void TeeStep::apply(Node node, std::vector<std::string> &values,
                    jsoncons::json &dict) {
  ExecutionContext local_ctx;
  apply(node, values, dict, local_ctx);
}

void TeeStep::apply(Node node, std::vector<std::string> &values,
                    jsoncons::json &dict, ExecutionContext &ctx) {
  std::vector<std::string> local_values = values;
  for (const auto &step : steps_) {
    if (!step) {
      continue;
    }
    step->apply(node, local_values, dict, ctx);
    if (ctx.skip_item || ctx.abort_scraper) {
      break;
    }
  }
}

void SaveValuesStep::apply(Node, std::vector<std::string> &, jsoncons::json &) {
  throw std::runtime_error(
      "save: requires an Extractor execution context");
}

void SaveValuesStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &, ExecutionContext &ctx) {
  ctx.slots[name_] = values;
}

void RestoreValuesStep::apply(Node, std::vector<std::string> &values,
                              jsoncons::json &) {
  values.clear();
}

void RestoreValuesStep::apply(Node, std::vector<std::string> &values,
                              jsoncons::json &, ExecutionContext &ctx) {
  const auto it = ctx.slots.find(name_);
  if (it != ctx.slots.end()) {
    values = it->second;
  } else {
    values.clear();
  }
}

namespace {

const jsoncons::json *resolve_dot_path(const jsoncons::json &root,
                                       std::string_view path) {
  const auto parts = split_dict_path(path, true);
  if (parts.empty()) {
    return nullptr;
  }
  const jsoncons::json *cur = &root;
  for (const auto &seg : parts) {
    if (!cur->is_object() || !cur->contains(seg)) {
      return nullptr;
    }
    cur = &cur->at(seg);
  }
  return cur;
}

} // namespace

jsoncons::json
CoalesceKeysStep::transform(const jsoncons::json &input) const {
  if (!input.is_object()) {
    return input;
  }
  jsoncons::json out = input;
  absl::flat_hash_set<std::string> targets;
  absl::flat_hash_set<std::string> to_drop;
  targets.reserve(rules_.size());

  for (const auto &[target, sources] : rules_) {
    targets.insert(target);
    const jsoncons::json *chosen = nullptr;
    for (const auto &src : sources) {
      const jsoncons::json *val = resolve_dot_path(input, src);
      if (val && !val->is_null() && !xsel::common::is_empty_json(*val)) {
        chosen = val;
        break;
      }
    }
    if (chosen) {
      insert_into_json(out, target, jsoncons::json(*chosen),
                       InsertOptions{InsertMode::Overwrite, false, true});
    }
    if (drop_sources_) {
      for (const auto &src : sources) {
        const auto parts = split_dict_path(src, true);
        if (parts.size() == 1) {
          to_drop.insert(parts[0]);
        }
      }
    }
  }

  if (drop_sources_) {
    for (const auto &src : to_drop) {
      if (!targets.contains(src)) {
        out.erase(src);
      }
    }
  }
  return out;
}

} // namespace scraper
