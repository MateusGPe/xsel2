#include "scraper/dsl/value.hpp"
#include "query/json_query.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace scraper::dsl {

DslValue::DslValue(std::string name, bool is_ident) {
  if (is_ident) {
    value = Identifier{std::move(name)};
  } else {
    value = std::move(name);
  }
}

std::string DslValue::to_string() const {
  if (is_null()) {
    return "null";
  }
  if (is_string()) {
    return get_string();
  }
  if (is_int()) {
    return std::to_string(get_int());
  }
  if (is_double()) {
    std::ostringstream oss;
    oss << get_double();
    return oss.str();
  }
  if (is_bool()) {
    return get_bool() ? "true" : "false";
  }
  if (is_identifier()) {
    return get_identifier();
  }
  if (is_object()) {
    return "<object>";
  }
  return "<list>";
}

int parse_strict_int(std::string_view s, std::string_view context) {
  if (s.empty()) {
    throw std::runtime_error("Expected integer in " + std::string(context) +
                             ", got empty string");
  }
  int val = 0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    throw std::runtime_error("Invalid integer '" + std::string(s) + "' in " +
                             std::string(context));
  }
  return val;
}

std::string as_str(const DslValue &v) {
  if (v.is_string()) {
    return v.get_string();
  }
  if (v.is_identifier()) {
    return v.get_identifier();
  }
  if (v.is_int()) {
    return std::to_string(v.get_int());
  }
  if (v.is_double()) {
    std::ostringstream oss;
    oss << v.get_double();
    return oss.str();
  }
  if (v.is_bool()) {
    return v.get_bool() ? "true" : "false";
  }
  if (v.is_null()) {
    return "null";
  }
  throw std::runtime_error("Expected scalar string/identifier in DSL");
}

bool as_bool(const DslValue &v) {
  if (v.is_bool()) {
    return v.get_bool();
  }
  if (v.is_int()) {
    return v.get_int() != 0;
  }
  if (v.is_string()) {
    if (v.get_string() == "true") {
      return true;
    }
    if (v.get_string() == "false") {
      return false;
    }
    throw std::runtime_error("Invalid boolean string in DSL: '" +
                             v.get_string() + "'");
  }
  throw std::runtime_error("Expected boolean in DSL");
}

int as_int(const DslValue &v) {
  if (v.is_int()) {
    return v.get_int();
  }
  if (v.is_double()) {
    const double d = v.get_double();
    if (!std::isfinite(d) ||
        d < static_cast<double>(std::numeric_limits<int>::min()) ||
        d > static_cast<double>(std::numeric_limits<int>::max())) {
      throw std::runtime_error("Double value out of integer range in DSL");
    }
    return static_cast<int>(d);
  }
  if (v.is_string()) {
    return parse_strict_int(v.get_string(), "DSL");
  }
  throw std::runtime_error("Expected integer in DSL");
}

size_t as_non_negative_size(const DslValue &v, std::string_view method) {
  const int val = as_int(v);
  if (val < 0) {
    throw std::runtime_error(std::string(method) +
                             "() requires a non-negative integer, got " +
                             std::to_string(val));
  }
  return static_cast<size_t>(val);
}

std::vector<std::string> as_str_list(const DslValue &v) {
  if (!v.is_list()) {
    return {as_str(v)};
  }
  std::vector<std::string> out;
  out.reserve(v.get_list().size());
  for (const auto &el : v.get_list()) {
    out.push_back(as_str(el));
  }
  return out;
}

jsoncons::json dsl_value_to_json(const DslValue &v, int depth) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL nesting depth exceeded");
  }
  if (v.is_null()) {
    return jsoncons::json::null();
  }
  if (v.is_string()) {
    return jsoncons::json(v.get_string());
  }
  if (v.is_int()) {
    return jsoncons::json(v.get_int());
  }
  if (v.is_double()) {
    return jsoncons::json(v.get_double());
  }
  if (v.is_bool()) {
    return jsoncons::json(v.get_bool());
  }
  if (v.is_identifier()) {
    return jsoncons::json(v.get_identifier());
  }
  if (v.is_list()) {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &el : v.get_list()) {
      arr.push_back(dsl_value_to_json(el, depth + 1));
    }
    return arr;
  }
  if (v.is_object()) {
    jsoncons::json obj = jsoncons::json::object();
    for (const auto &[k, val] : v.get_object()) {
      obj.insert_or_assign(k, dsl_value_to_json(val, depth + 1));
    }
    return obj;
  }
  return jsoncons::json::null();
}

jsoncons::json as_json_document(const DslValue &v) {
  if (v.is_string()) {
    return xsel::query::parse_json(v.get_string());
  }
  return dsl_value_to_json(v);
}

const DslValue *find_arg(const DslArgList &args, size_t idx,
                         std::string_view name) {
  if (!name.empty()) {
    auto it = args.named.find(name);
    if (it != args.named.end()) {
      if (idx < args.positional.size()) {
        throw std::runtime_error("Argument '" + std::string(name) +
                                 "' passed both positionally and by keyword");
      }
      args.named_used.emplace(name);
      return &it->second;
    }
  }
  if (idx < args.positional.size()) {
    if (idx + 1 > args.max_positional_used) {
      args.max_positional_used = idx + 1;
    }
    return &args.positional[idx];
  }
  return nullptr;
}

void validate_args_consumed(const DslArgList &args, std::string_view method) {
  if (args.positional.size() > args.max_positional_used) {
    throw std::runtime_error(
        std::string(method) +
        "() received too many positional arguments (expected at most " +
        std::to_string(args.max_positional_used) + ", got " +
        std::to_string(args.positional.size()) + ")");
  }
  if (args.named_used.size() < args.named.size()) {
    for (const auto &[k, _] : args.named) {
      if (!args.named_used.contains(k)) {
        throw std::runtime_error(std::string(method) +
                                 "() received unknown keyword argument '" + k +
                                 "'");
      }
    }
  }
}

const DslValue &require_arg(const DslArgList &args, size_t idx,
                            std::string_view name, std::string_view method) {
  const DslValue *v = find_arg(args, idx, name);
  if (!v) {
    throw std::runtime_error(std::string(method) +
                             "() missing required argument '" +
                             std::string(name) + "'");
  }
  return *v;
}

std::string opt_str(const DslArgList &args, size_t idx, std::string_view name,
                    std::string def) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_str(*v);
  }
  return def;
}

bool opt_bool(const DslArgList &args, size_t idx, std::string_view name,
              bool def) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_bool(*v);
  }
  return def;
}

int opt_int(const DslArgList &args, size_t idx, std::string_view name,
            int def) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_int(*v);
  }
  return def;
}

size_t opt_non_negative_size(const DslArgList &args, size_t idx,
                             std::string_view name, std::string_view method,
                             size_t def) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_non_negative_size(*v, method);
  }
  return def;
}

} // namespace scraper::dsl
