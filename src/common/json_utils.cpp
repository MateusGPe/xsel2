#include "common/json_utils.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
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
  check_json_depth(depth, "canonical_json_key");
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

int compare_int_double(int64_t i, double d) noexcept {
  if (std::isnan(d)) {
    return -1;
  }
  // -2^63 is exactly representable in IEEE-754 double; +2^63 is also exact.
  constexpr double kMinInt64AsDouble = -9223372036854775808.0;
  constexpr double kMaxInt64Plus1AsDouble = 9223372036854775808.0;
  if (d < kMinInt64AsDouble) {
    return 1;
  }
  if (d == kMinInt64AsDouble) {
    return (i == std::numeric_limits<int64_t>::min()) ? 0 : 1;
  }
  if (d >= kMaxInt64Plus1AsDouble) {
    return -1;
  }
  const double t = std::trunc(d);
  const auto di = static_cast<int64_t>(t);
  if (i < di)
    return -1;
  if (i > di)
    return 1;
  const double frac = d - t;
  if (frac > 0.0)
    return -1;
  if (frac < 0.0)
    return 1;
  return 0;
}

int compare_uint_double(uint64_t u, double d) noexcept {
  if (std::isnan(d)) {
    return -1;
  }
  if (d < 0.0) {
    return 1;
  }
  constexpr double kMaxUint64Plus1AsDouble = 18446744073709551616.0;
  if (d >= kMaxUint64Plus1AsDouble) {
    return -1;
  }
  const double t = std::trunc(d);
  const auto du = static_cast<uint64_t>(t);
  if (u < du)
    return -1;
  if (u > du)
    return 1;
  return (d - t > 0.0) ? -1 : 0;
}

int json_type_rank(const jsoncons::json &v) noexcept {
  if (v.is_null())
    return 0;
  if (v.is_bool())
    return 1;
  if (v.is_number())
    return 2;
  if (v.is_string())
    return 3;
  if (v.is_array())
    return 4;
  if (v.is_object())
    return 5;
  return 6;
}

int compare_numbers(const jsoncons::json &a, const jsoncons::json &b) noexcept {
  if (a.is_int64()) {
    const int64_t ia = a.as<int64_t>();
    if (b.is_int64()) {
      const int64_t ib = b.as<int64_t>();
      return (ia < ib) ? -1 : (ia > ib ? 1 : 0);
    }
    if (b.is_uint64()) {
      return compare_int_uint(ia, b.as<uint64_t>());
    }
    return compare_int_double(ia, b.as<double>());
  }
  if (a.is_uint64()) {
    const uint64_t ua = a.as<uint64_t>();
    if (b.is_uint64()) {
      const uint64_t ub = b.as<uint64_t>();
      return (ua < ub) ? -1 : (ua > ub ? 1 : 0);
    }
    if (b.is_int64()) {
      return -compare_int_uint(b.as<int64_t>(), ua);
    }
    return compare_uint_double(ua, b.as<double>());
  }
  if (b.is_int64()) {
    return -compare_int_double(b.as<int64_t>(), a.as<double>());
  }
  if (b.is_uint64()) {
    return -compare_uint_double(b.as<uint64_t>(), a.as<double>());
  }
  const double da = a.as<double>();
  const double db = b.as<double>();
  const bool na = std::isnan(da);
  const bool nb = std::isnan(db);
  if (na || nb) {
    return na == nb ? 0 : (na ? 1 : -1);
  }
  if (da < db)
    return -1;
  if (da > db)
    return 1;
  return 0;
}

} // namespace

void check_json_depth(int depth, std::string_view context) {
  if (depth > MAX_JSON_DEPTH) {
    throw std::runtime_error("Maximum JSON recursion depth exceeded in " +
                             std::string(context));
  }
}

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

int compare_json(const jsoncons::json &a, const jsoncons::json &b, int depth) {
  check_json_depth(depth, "compare_json");
  const int rank_a = json_type_rank(a);
  const int rank_b = json_type_rank(b);
  if (rank_a != rank_b) {
    return (rank_a < rank_b) ? -1 : 1;
  }

  switch (rank_a) {
  case 0:
    return 0;
  case 1: {
    const bool ba = a.as<bool>();
    const bool bb = b.as<bool>();
    return (ba == bb) ? 0 : (ba ? 1 : -1);
  }
  case 2:
    return compare_numbers(a, b);
  case 3: {
    const std::string_view sa = a.as_string_view();
    const std::string_view sb = b.as_string_view();
    return (sa < sb) ? -1 : (sa > sb ? 1 : 0);
  }
  case 4: {
    const size_t min_len = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_len; ++i) {
      const int cmp = compare_json(a[i], b[i], depth + 1);
      if (cmp != 0) {
        return cmp;
      }
    }
    return (a.size() < b.size()) ? -1 : (a.size() > b.size() ? 1 : 0);
  }
  case 5: {
    std::vector<std::string_view> keys_a;
    keys_a.reserve(a.size());
    for (const auto &kv : a.object_range()) {
      keys_a.push_back(kv.key());
    }
    std::vector<std::string_view> keys_b;
    keys_b.reserve(b.size());
    for (const auto &kv : b.object_range()) {
      keys_b.push_back(kv.key());
    }
    std::sort(keys_a.begin(), keys_a.end());
    std::sort(keys_b.begin(), keys_b.end());

    const size_t min_keys = std::min(keys_a.size(), keys_b.size());
    for (size_t i = 0; i < min_keys; ++i) {
      if (keys_a[i] < keys_b[i]) {
        return -1;
      }
      if (keys_a[i] > keys_b[i]) {
        return 1;
      }
      const int val_cmp =
          compare_json(a.at(keys_a[i]), b.at(keys_b[i]), depth + 1);
      if (val_cmp != 0) {
        return val_cmp;
      }
    }
    return (keys_a.size() < keys_b.size())
               ? -1
               : (keys_a.size() > keys_b.size() ? 1 : 0);
  }
  default: {
    const std::string sa = json_to_string(a);
    const std::string sb = json_to_string(b);
    return (sa < sb) ? -1 : (sa > sb ? 1 : 0);
  }
  }
}

std::string next_unique_key(const jsoncons::json &obj, std::string_view base) {
  if (!obj.is_object() || !obj.contains(base)) {
    return std::string(base);
  }
  std::string candidate;
  candidate.reserve(base.size() + 21);
  char buf[24];
  for (size_t idx = 0;; ++idx) {
    const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), idx);
    (void)ec;
    candidate.assign(base);
    candidate.push_back('_');
    candidate.append(buf, ptr);
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
