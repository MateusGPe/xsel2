#include "scraper/merge.hpp"
#include <stdexcept>

namespace scraper {

namespace {

void check_depth(int depth, const char *fn) {
  xsel::common::check_json_depth(depth, fn);
}

void combine_values_as_array(jsoncons::json &existing,
                             jsoncons::json &&incoming) {
  if (existing.is_array()) {
    if (incoming.is_array()) {
      for (auto &el : incoming.array_range()) {
        existing.push_back(std::move(el));
      }
    } else {
      existing.push_back(std::move(incoming));
    }
    return;
  }

  jsoncons::json arr = jsoncons::json::array();
  arr.push_back(std::move(existing));
  if (incoming.is_array()) {
    for (auto &el : incoming.array_range()) {
      arr.push_back(std::move(el));
    }
  } else {
    arr.push_back(std::move(incoming));
  }
  existing = std::move(arr);
}

void assign_key_with_mode(jsoncons::json &obj, std::string_view key,
                          jsoncons::json &&value, InsertMode mode, int depth) {
  if (!obj.contains(key)) {
    obj.insert_or_assign(std::string(key), std::move(value));
    return;
  }

  switch (mode) {
  case InsertMode::Overwrite:
    obj.insert_or_assign(std::string(key), std::move(value));
    break;
  case InsertMode::AppendToList:
    combine_values_as_array(obj.at(key), std::move(value));
    break;
  case InsertMode::Merge:
    if (obj.at(key).is_object() && value.is_object()) {
      merge_into(obj.at(key), std::move(value), InsertMode::Merge, depth + 1);
    } else {
      combine_values_as_array(obj.at(key), std::move(value));
    }
    break;
  case InsertMode::Default:
  case InsertMode::UniqueSuffix: {
    std::string unique_key = xsel::common::next_unique_key(obj, key);
    obj.insert_or_assign(std::move(unique_key), std::move(value));
    break;
  }
  }
}

} // namespace

std::vector<std::string> split_dict_path(std::string_view key,
                                         bool nested_path) {
  std::vector<std::string> segments;
  if (!nested_path) {
    segments.emplace_back(key);
    return segments;
  }

  std::string current;
  current.reserve(key.size());
  for (size_t i = 0; i < key.size(); ++i) {
    if (key[i] == '\\' && i + 1 < key.size() && key[i + 1] == '.') {
      current.push_back('.');
      ++i;
    } else if (key[i] == '.') {
      segments.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(key[i]);
    }
  }
  segments.push_back(std::move(current));
  return segments;
}

void insert_into_json(jsoncons::json &dest, std::string_view key,
                      jsoncons::json &&value, const InsertOptions &opts) {
  if (!opts.allow_empty && xsel::common::is_empty_json(value)) {
    return;
  }

  if (dest.is_null()) {
    dest = jsoncons::json::object();
  }

  if (dest.is_array()) {
    if (key.empty()) {
      dest.push_back(std::move(value));
    } else {
      jsoncons::json item = jsoncons::json::object();
      insert_into_json(item, key, std::move(value), opts);
      if (!item.empty()) {
        dest.push_back(std::move(item));
      }
    }
    return;
  }

  if (!dest.is_object()) {
    return;
  }

  const auto segments = split_dict_path(key, opts.nested_path);
  if (segments.empty()) {
    return;
  }
  check_depth(static_cast<int>(segments.size()), "insert_into_json");

  jsoncons::json *cursor = &dest;
  for (size_t i = 0; i + 1 < segments.size(); ++i) {
    const std::string &seg = segments[i];
    if (!cursor->contains(seg) || !cursor->at(seg).is_object()) {
      cursor->insert_or_assign(seg, jsoncons::json::object());
    }
    cursor = &cursor->at(seg);
  }

  assign_key_with_mode(*cursor, segments.back(), std::move(value), opts.mode,
                       static_cast<int>(segments.size()));
}

bool has_no_common_key(const jsoncons::json &obj1, const jsoncons::json &obj2) {
  if (!obj1.is_object() || !obj2.is_object()) {
    return true;
  }
  for (const auto &kv : obj1.object_range()) {
    if (obj2.contains(kv.key())) {
      return false;
    }
  }
  return true;
}

void preprocess_key_value(jsoncons::json &val, InsertMode mode, int depth) {
  check_depth(depth, "preprocess_key_value");

  if (val.is_array()) {
    for (auto &el : val.array_range()) {
      preprocess_key_value(el, mode, depth + 1);
    }
    return;
  }

  if (!val.is_object()) {
    return;
  }

  // Recurse into children first.
  for (auto &kv : val.object_range()) {
    preprocess_key_value(kv.value(), mode, depth + 1);
  }

  const bool has_key = val.contains("@key");
  const bool has_val = val.contains("@value");
  if (!has_key && !has_val) {
    return;
  }

  if (has_key && has_val) {
    std::string k = xsel::common::json_to_string(val.at("@key"));
    jsoncons::json v = std::move(val.at("@value"));
    val.erase("@key");
    val.erase("@value");
    if (!k.empty() && !xsel::common::is_empty_json(v)) {
      assign_key_with_mode(val, k, std::move(v), mode, depth + 1);
    }
  } else {
    val.erase("@key");
    val.erase("@value");
  }
}

void merge_into(jsoncons::json &dest, const jsoncons::json &other, int depth) {
  merge_into(dest, other, InsertMode::Default, depth);
}

void merge_into(jsoncons::json &dest, jsoncons::json &&other, int depth) {
  merge_into(dest, std::move(other), InsertMode::Default, depth);
}

void merge_into(jsoncons::json &dest, const jsoncons::json &other,
                InsertMode mode, int depth) {
  jsoncons::json copy = other;
  merge_into(dest, std::move(copy), mode, depth);
}

void merge_into(jsoncons::json &dest, jsoncons::json &&other, InsertMode mode,
                int depth) {
  check_depth(depth, "merge_into");

  if (xsel::common::is_empty_json(other)) {
    return;
  }

  preprocess_key_value(other, mode, depth + 1);
  if (xsel::common::is_empty_json(other)) {
    return;
  }

  if (dest.is_null()) {
    dest = std::move(other);
    return;
  }

  if (dest.is_object() && other.is_object()) {
    if (mode == InsertMode::Merge || has_no_common_key(dest, other)) {
      for (auto &kv : other.object_range()) {
        assign_key_with_mode(dest, kv.key(), std::move(kv.value()),
                             mode == InsertMode::Merge ? InsertMode::Merge
                                                       : mode,
                             depth + 1);
      }
    } else {
      combine_values_as_array(dest, std::move(other));
    }
    return;
  }

  if (dest.is_array()) {
    if (other.is_array()) {
      for (auto &el : other.array_range()) {
        dest.push_back(std::move(el));
      }
    } else {
      dest.push_back(std::move(other));
    }
    return;
  }

  combine_values_as_array(dest, std::move(other));
}

void merge_item_at_path(jsoncons::json &dest,
                        const std::vector<std::string> &path_segments,
                        jsoncons::json &&item) {
  if (xsel::common::is_empty_json(item)) {
    return;
  }
  check_depth(static_cast<int>(path_segments.size()), "merge_item_at_path");

  if (dest.is_object() || dest.is_null()) {
    if (path_segments.empty()) {
      merge_into(dest, std::move(item));
      return;
    }
    if (dest.is_null()) {
      dest = jsoncons::json::object();
    }
    jsoncons::json *target = &dest;
    for (size_t i = 0; i + 1 < path_segments.size(); ++i) {
      if (!target->contains(path_segments[i]) ||
          !target->at(path_segments[i]).is_object()) {
        target->insert_or_assign(path_segments[i], jsoncons::json::object());
      }
      target = &target->at(path_segments[i]);
    }
    if (!target->contains(path_segments.back())) {
      target->insert_or_assign(path_segments.back(), jsoncons::json::null());
    }
    merge_into(target->at(path_segments.back()), std::move(item),
               static_cast<int>(path_segments.size()));
    return;
  }

  if (dest.is_array()) {
    if (path_segments.empty()) {
      merge_into(dest, std::move(item));
      return;
    }
    preprocess_key_value(item);
    if (!xsel::common::is_empty_json(item)) {
      jsoncons::json wrapped = std::move(item);
      for (size_t i = path_segments.size(); i > 0; --i) {
        jsoncons::json obj = jsoncons::json::object();
        obj.insert_or_assign(path_segments[i - 1], std::move(wrapped));
        wrapped = std::move(obj);
      }
      merge_into(dest, std::move(wrapped));
    }
  }
}

} // namespace scraper
