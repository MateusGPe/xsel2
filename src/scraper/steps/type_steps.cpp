#include "scraper/steps/type_steps.hpp"
#include "common/json_utils.hpp"
#include "digger/scalar_parser.hpp"
#include <absl/strings/ascii.h>
#include <absl/time/time.h>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <stdexcept>

namespace scraper {

namespace {

bool is_digit_char(char c) noexcept {
  return c >= '0' && c <= '9';
}

std::optional<bool> parse_bool_literal(std::string_view s) {
  const std::string lower =
      absl::AsciiStrToLower(absl::StripAsciiWhitespace(s));
  if (lower == "true" || lower == "1" || lower == "yes" || lower == "y" ||
      lower == "on" || lower == "in stock" || lower == "available") {
    return true;
  }
  if (lower == "false" || lower == "0" || lower == "no" || lower == "n" ||
      lower == "off" || lower == "out of stock" || lower == "unavailable") {
    return false;
  }
  return std::nullopt;
}

std::optional<int64_t> parse_int_literal(std::string_view s) {
  const std::string_view trimmed = absl::StripAsciiWhitespace(s);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  int64_t val = 0;
  const auto [ptr, ec] =
      std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), val);
  if (ec == std::errc{} && ptr == trimmed.data() + trimmed.size()) {
    return val;
  }
  return std::nullopt;
}

std::optional<double> parse_float_literal(std::string_view s) {
  const std::string_view trimmed = absl::StripAsciiWhitespace(s);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  double val = 0.0;
  const auto [ptr, ec] =
      std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), val);
  if (ec == std::errc{} && ptr == trimmed.data() + trimmed.size() &&
      std::isfinite(val)) {
    return val;
  }
  return std::nullopt;
}

void write_typed_values_to_dict(jsoncons::json &dict, std::string_view key,
                                std::vector<jsoncons::json> &&items,
                                const InsertOptions &opts) {
  if (items.size() == 1) {
    insert_into_json(dict, key, std::move(items[0]), opts);
  } else if (items.size() > 1) {
    jsoncons::json arr = jsoncons::json::array();
    for (auto &item : items) {
      arr.push_back(std::move(item));
    }
    insert_into_json(dict, key, std::move(arr), opts);
  }
}

} // namespace

ValueType parse_value_type_name(std::string_view type_name) {
  const std::string lower = absl::AsciiStrToLower(type_name);
  if (lower == "string" || lower == "str") {
    return ValueType::String;
  }
  if (lower == "int" || lower == "integer") {
    return ValueType::Int;
  }
  if (lower == "float" || lower == "double") {
    return ValueType::Float;
  }
  if (lower == "number" || lower == "num") {
    return ValueType::Number;
  }
  if (lower == "bool" || lower == "boolean") {
    return ValueType::Bool;
  }
  if (lower == "auto") {
    return ValueType::Auto;
  }
  throw std::runtime_error("Unknown ValueType: " + std::string(type_name));
}

std::optional<std::string> extract_number_string(std::string_view input,
                                                 std::string_view decimal,
                                                 std::string_view thousands,
                                                 bool allow_float) {
  const std::string_view trimmed = absl::StripAsciiWhitespace(input);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  // Locate start of numeric token (first digit, or '-'/'+'/'(' preceding a digit).
  size_t i = 0;
  bool negative = false;
  bool accounting_parens = false;

  while (i < trimmed.size()) {
    if (is_digit_char(trimmed[i])) {
      break;
    }
    if (allow_float && !decimal.empty() &&
        trimmed.substr(i).starts_with(decimal) &&
        i + decimal.size() < trimmed.size() &&
        is_digit_char(trimmed[i + decimal.size()])) {
      break;
    }
    if (trimmed[i] == '-' && i + 1 < trimmed.size() &&
        (is_digit_char(trimmed[i + 1]) ||
         (allow_float && !decimal.empty() &&
          trimmed.substr(i + 1).starts_with(decimal)))) {
      negative = true;
      ++i;
      break;
    }
    if (trimmed[i] == '(' && i + 1 < trimmed.size() &&
        (is_digit_char(trimmed[i + 1]) || trimmed[i + 1] == '$' ||
         trimmed[i + 1] == ' ' ||
         (allow_float && !decimal.empty() &&
          trimmed.substr(i + 1).starts_with(decimal)))) {
      accounting_parens = true;
    } else if (trimmed[i] == ')') {
      accounting_parens = false;
    }
    ++i;
  }

  if (i >= trimmed.size()) {
    return std::nullopt;
  }

  std::string digits_out;
  bool seen_digit = false;
  bool seen_decimal = false;
  while (i < trimmed.size()) {
    if (is_digit_char(trimmed[i])) {
      digits_out.push_back(trimmed[i]);
      seen_digit = true;
      ++i;
    } else if (!thousands.empty() && trimmed.substr(i).starts_with(thousands)) {
      i += thousands.size();
    } else if (allow_float && !seen_decimal && !decimal.empty() &&
               trimmed.substr(i).starts_with(decimal)) {
      if (!seen_digit) {
        digits_out.push_back('0');
      }
      digits_out.push_back('.');
      seen_decimal = true;
      i += decimal.size();
    } else {
      break;
    }
  }

  if (!seen_digit) {
    return std::nullopt;
  }
  if (digits_out.back() == '.') {
    digits_out.pop_back();
  }
  const bool is_neg =
      negative ||
      (accounting_parens && trimmed.find(')', i) != std::string_view::npos);
  if (is_neg) {
    return "-" + digits_out;
  }
  return digits_out;
}

jsoncons::json coerce_string_to_json(std::string_view s, ValueType type) {
  const std::string_view trimmed = absl::StripAsciiWhitespace(s);
  switch (type) {
  case ValueType::String:
    return jsoncons::json(std::string(s));
  case ValueType::Int: {
    if (auto iv = parse_int_literal(trimmed)) {
      return jsoncons::json(*iv);
    }
    if (auto dv = parse_float_literal(trimmed)) {
      constexpr double kMinInt64 = -9223372036854775808.0;
      constexpr double kMaxInt64Plus1 = 9223372036854775808.0;
      if (*dv >= kMinInt64 && *dv < kMaxInt64Plus1) {
        return jsoncons::json(static_cast<int64_t>(*dv));
      }
    }
    return jsoncons::json::null();
  }
  case ValueType::Float: {
    if (auto dv = parse_float_literal(trimmed)) {
      return jsoncons::json(*dv);
    }
    return jsoncons::json::null();
  }
  case ValueType::Number: {
    jsoncons::json parsed = digger::detail::parse_number_literal(trimmed);
    if (parsed.is_number()) {
      return parsed;
    }
    return jsoncons::json::null();
  }
  case ValueType::Bool: {
    if (auto bv = parse_bool_literal(trimmed)) {
      return jsoncons::json(*bv);
    }
    return jsoncons::json::null();
  }
  case ValueType::Auto: {
    if (trimmed.empty()) {
      return jsoncons::json(std::string(s));
    }
    const std::string lower = absl::AsciiStrToLower(trimmed);
    if (lower == "true") {
      return jsoncons::json(true);
    }
    if (lower == "false") {
      return jsoncons::json(false);
    }
    if (lower == "null") {
      return jsoncons::json::null();
    }
    return digger::detail::parse_number_literal(trimmed);
  }
  }
  return jsoncons::json(std::string(s));
}

void ParseNumberStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  std::vector<std::string> normalized;
  normalized.reserve(values.size());
  for (const auto &v : values) {
    if (auto num = extract_number_string(v, decimal_, thousands_, allow_float_)) {
      normalized.push_back(std::move(*num));
    }
  }

  if (!key_.empty()) {
    std::vector<jsoncons::json> typed;
    typed.reserve(normalized.size());
    for (const auto &n : normalized) {
      typed.push_back(coerce_string_to_json(
          n, allow_float_ ? ValueType::Number : ValueType::Int));
    }
    write_typed_values_to_dict(dict, key_, std::move(typed), opts_);
    values.clear();
  } else {
    values = std::move(normalized);
  }
}

void CastValuesStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &dict) {
  std::vector<jsoncons::json> typed;
  std::vector<std::string> normalized;
  typed.reserve(values.size());
  normalized.reserve(values.size());

  for (const auto &v : values) {
    jsoncons::json coerced = coerce_string_to_json(v, target_type_);
    if (coerced.is_null() && default_val_.has_value()) {
      coerced = *default_val_;
    }
    if (!coerced.is_null() || default_val_.has_value()) {
      normalized.push_back(xsel::common::json_to_string(coerced));
      typed.push_back(std::move(coerced));
    }
  }

  if (!key_.empty()) {
    write_typed_values_to_dict(dict, key_, std::move(typed), opts_);
    values.clear();
  } else {
    values = std::move(normalized);
  }
}

jsoncons::json CastKeysStep::transform(const jsoncons::json &input) const {
  return cast_impl(input, 0);
}

jsoncons::json CastKeysStep::cast_impl(const jsoncons::json &input,
                                       int depth) const {
  xsel::common::check_json_depth(depth, "CastKeysStep");
  if (input.is_array()) {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &elem : input.array_range()) {
      arr.push_back(cast_impl(elem, depth + 1));
    }
    return arr;
  }
  if (!input.is_object()) {
    return input;
  }

  jsoncons::json out = input;
  for (const auto &[key, target_type] : schema_) {
    if (!out.contains(key)) {
      continue;
    }
    const auto &val = out[key];
    if (val.is_string()) {
      out.insert_or_assign(key,
                           coerce_string_to_json(val.as_string_view(), target_type));
    } else if (val.is_array()) {
      jsoncons::json arr = jsoncons::json::array();
      for (const auto &elem : val.array_range()) {
        if (elem.is_string()) {
          arr.push_back(coerce_string_to_json(elem.as_string_view(), target_type));
        } else {
          arr.push_back(elem);
        }
      }
      out.insert_or_assign(key, std::move(arr));
    }
  }
  return out;
}

namespace {

bool parse_with_auto_formats(std::string_view text, absl::TimeZone tz,
                            std::string_view explicit_fmt, absl::Time *out) {
  std::string err;
  if (!explicit_fmt.empty()) {
    return absl::ParseTime(explicit_fmt, text, tz, out, &err);
  }
  static constexpr std::array<std::string_view, 15> kFormats = {
      "%Y-%m-%dT%H:%M:%E*S%Ez",
      "%Y-%m-%dT%H:%M:%E*SZ",
      "%Y-%m-%dT%H:%M:%S",
      "%Y-%m-%d %H:%M:%S%Ez",
      "%Y-%m-%d %H:%M:%S",
      "%a, %d %b %Y %H:%M:%S %Z",
      "%a, %d %b %Y %H:%M:%S %z",
      "%Y-%m-%d",
      "%Y/%m/%d",
      "%d/%m/%Y",
      "%m/%d/%Y",
      "%d-%m-%Y",
      "%b %d, %Y",
      "%B %d, %Y",
      "%d %b %Y",
  };
  for (std::string_view fmt : kFormats) {
    if (absl::ParseTime(fmt, text, tz, out, &err)) {
      return true;
    }
  }
  return false;
}

} // namespace

void ParseDateStep::apply(Node, std::vector<std::string> &values,
                          jsoncons::json &dict) {
  absl::TimeZone tz = absl::UTCTimeZone();
  if (!tz_.empty() && tz_ != "UTC") {
    if (!absl::LoadTimeZone(tz_, &tz)) {
      throw std::invalid_argument("parse_date: unknown timezone '" + tz_ + "'");
    }
  }

  std::vector<std::string> out_values;
  std::vector<jsoncons::json> out_json;
  out_values.reserve(values.size());
  out_json.reserve(values.size());

  for (const auto &raw : values) {
    std::string_view trimmed = absl::StripAsciiWhitespace(raw);
    if (trimmed.empty()) {
      continue;
    }
    absl::Time parsed;
    if (!parse_with_auto_formats(trimmed, tz, fmt_, &parsed)) {
      continue;
    }
    if (output_ == "unix") {
      const int64_t sec = absl::ToUnixSeconds(parsed);
      out_values.push_back(std::to_string(sec));
      out_json.emplace_back(sec);
    } else if (output_ == "unix_ms") {
      const int64_t ms = absl::ToUnixMillis(parsed);
      out_values.push_back(std::to_string(ms));
      out_json.emplace_back(ms);
    } else if (output_ == "date") {
      std::string formatted = absl::FormatTime("%Y-%m-%d", parsed, tz);
      out_values.push_back(formatted);
      out_json.emplace_back(std::move(formatted));
    } else if (output_.empty() || output_ == "iso") {
      const std::string_view iso_fmt =
          (tz_ == "UTC" || tz_.empty()) ? "%Y-%m-%dT%H:%M:%SZ"
                                        : "%Y-%m-%dT%H:%M:%S%Ez";
      std::string formatted = absl::FormatTime(iso_fmt, parsed, tz);
      out_values.push_back(formatted);
      out_json.emplace_back(std::move(formatted));
    } else {
      std::string formatted = absl::FormatTime(output_, parsed, tz);
      out_values.push_back(formatted);
      out_json.emplace_back(std::move(formatted));
    }
  }

  values = std::move(out_values);

  if (!key_.empty() && !out_json.empty()) {
    if (out_json.size() == 1) {
      insert_into_json(dict, key_, std::move(out_json.front()), opts_);
    } else {
      jsoncons::json arr = jsoncons::json::array();
      for (auto &item : out_json) {
        arr.push_back(std::move(item));
      }
      insert_into_json(dict, key_, std::move(arr), opts_);
    }
  }
}

} // namespace scraper
