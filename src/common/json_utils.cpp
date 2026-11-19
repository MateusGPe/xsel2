#include "common/json_utils.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace xsel::common {

bool is_empty_json(const jsoncons::json &val) noexcept {
  if (val.is_null()) {
    return true;
  }
  if (val.is_string()) {
    return val.as_string_view().empty();
  }
  if (val.is_array() || val.is_object()) {
    return val.empty();
  }
  return false;
}

std::string canonical_json_key(const jsoncons::json &val, int depth) {
  if (depth > MAX_JSON_DEPTH) {
    throw std::runtime_error(
        "Maximum JSON nesting depth exceeded in canonical_json_key");
  }
  if (val.is_object()) {
    std::vector<std::string_view> keys;
    keys.reserve(val.size());
    for (const auto &kv : val.object_range()) {
      keys.push_back(kv.key());
    }
    std::sort(keys.begin(), keys.end());
    std::string out = "{";
    for (size_t i = 0; i < keys.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      std::string quoted_key;
      jsoncons::json(keys[i]).dump(quoted_key);
      out += quoted_key;
      out += ':';
      out += canonical_json_key(val.at(keys[i]), depth + 1);
    }
    out += '}';
    return out;
  }
  if (val.is_array()) {
    std::string out = "[";
    for (size_t i = 0; i < val.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      out += canonical_json_key(val[i], depth + 1);
    }
    out += ']';
    return out;
  }
  std::string out;
  val.dump(out);
  return out;
}

int compare_json(const jsoncons::json &a, const jsoncons::json &b) {
  if (a.is_number() && b.is_number()) {
    const double da = a.as<double>();
    const double db = b.as<double>();
    if (da < db)
      return -1;
    if (da > db)
      return 1;
    return 0;
  }
  const std::string sa = json_to_string(a);
  const std::string sb = json_to_string(b);
  if (sa < sb)
    return -1;
  if (sa > sb)
    return 1;
  return 0;
}

std::string next_unique_key(const jsoncons::json &obj, std::string_view base) {
  if (!obj.is_object() || !obj.contains(base)) {
    return std::string(base);
  }
  for (size_t idx = 0;; ++idx) {
    std::string candidate;
    candidate.reserve(base.size() + 12);
    candidate.append(base);
    candidate.push_back('_');
    candidate.append(std::to_string(idx));
    if (!obj.contains(candidate)) {
      return candidate;
    }
  }
}

std::string json_to_string(const jsoncons::json &val) {
  if (val.is_string()) {
    return val.as<std::string>();
  }
  std::string out;
  val.dump(out);
  return out;
}

} // namespace xsel::common
