#include "common/json_utils.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace xsel::common {

namespace {

void append_json_escaped_string(std::string &out, std::string_view sv) {
  static constexpr char kHex[] = "0123456789abcdef";
  out.push_back('"');
  for (unsigned char c : sv) {
    switch (c) {
    case '"':
      out.append("\\\"");
      break;
    case '\\':
      out.append("\\\\");
      break;
    case '\b':
      out.append("\\b");
      break;
    case '\f':
      out.append("\\f");
      break;
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\t':
      out.append("\\t");
      break;
    default:
      if (c < 0x20) {
        out.append("\\u00");
        out.push_back(kHex[(c >> 4) & 0x0F]);
        out.push_back(kHex[c & 0x0F]);
      } else {
        out.push_back(static_cast<char>(c));
      }
      break;
    }
  }
  out.push_back('"');
}

void canonical_json_key_into(const jsoncons::json &val, int depth,
                             std::string &out) {
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
    out.push_back('{');
    for (size_t i = 0; i < keys.size(); ++i) {
      if (i > 0) {
        out.push_back(',');
      }
      append_json_escaped_string(out, keys[i]);
      out.push_back(':');
      canonical_json_key_into(val.at(keys[i]), depth + 1, out);
    }
    out.push_back('}');
    return;
  }
  if (val.is_array()) {
    out.push_back('[');
    for (size_t i = 0; i < val.size(); ++i) {
      if (i > 0) {
        out.push_back(',');
      }
      canonical_json_key_into(val[i], depth + 1, out);
    }
    out.push_back(']');
    return;
  }
  val.dump(out);
}

int compare_int_uint(int64_t s, uint64_t u) noexcept {
  if (s < 0) {
    return -1;
  }
  const auto su = static_cast<uint64_t>(s);
  if (su < u)
    return -1;
  if (su > u)
    return 1;
  return 0;
}

} // namespace

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
  std::string out;
  canonical_json_key_into(val, depth, out);
  return out;
}

int compare_json(const jsoncons::json &a, const jsoncons::json &b) {
  if (a.is_number() && b.is_number()) {
    if (a.is_int64() && b.is_int64()) {
      const int64_t ia = a.as<int64_t>();
      const int64_t ib = b.as<int64_t>();
      return (ia < ib) ? -1 : (ia > ib ? 1 : 0);
    }
    if (a.is_uint64() && b.is_uint64()) {
      const uint64_t ua = a.as<uint64_t>();
      const uint64_t ub = b.as<uint64_t>();
      return (ua < ub) ? -1 : (ua > ub ? 1 : 0);
    }
    if (a.is_int64() && b.is_uint64()) {
      return compare_int_uint(a.as<int64_t>(), b.as<uint64_t>());
    }
    if (a.is_uint64() && b.is_int64()) {
      return -compare_int_uint(b.as<int64_t>(), a.as<uint64_t>());
    }
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
