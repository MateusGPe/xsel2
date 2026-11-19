#pragma once

#include "digger/token.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <jsoncons/json.hpp>
#include <string>
#include <string_view>

namespace digger {

namespace detail {

[[nodiscard]] inline constexpr bool is_space_char(char c) noexcept {
  const auto uc = static_cast<unsigned char>(c);
  return uc == ' ' || (uc >= '\t' && uc <= '\r');
}

[[nodiscard]] inline std::string normalize_whitespace(std::string_view input) {
  size_t pos = 0;
  while (pos < input.size() && is_space_char(input[pos])) {
    ++pos;
  }
  size_t end = input.size();
  while (end > pos && is_space_char(input[end - 1])) {
    --end;
  }

  std::string out;
  if (pos >= end) {
    return out;
  }
  out.reserve(end - pos);

  bool in_space = false;
  while (pos < end) {
    if (is_space_char(input[pos])) {
      in_space = true;
    } else {
      if (in_space) {
        out.push_back(' ');
        in_space = false;
      }
      out.push_back(input[pos]);
    }
    ++pos;
  }
  return out;
}

[[nodiscard]] inline jsoncons::json parse_number_literal(std::string_view text) {
  if (text.empty()) {
    return jsoncons::json(std::string(text));
  }

  const bool has_float_markers =
      text.find('.') != std::string_view::npos ||
      text.find('e') != std::string_view::npos ||
      text.find('E') != std::string_view::npos;

  if (!has_float_markers) {
    int64_t ival = 0;
    const auto [ptr, ec] =
        std::from_chars(text.data(), text.data() + text.size(), ival);
    if (ec == std::errc{} && ptr == text.data() + text.size()) {
      return jsoncons::json(ival);
    }
    if (ec == std::errc::result_out_of_range && text.front() != '-') {
      uint64_t uval = 0;
      const auto [uptr, uec] =
          std::from_chars(text.data(), text.data() + text.size(), uval);
      if (uec == std::errc{} && uptr == text.data() + text.size()) {
        return jsoncons::json(uval);
      }
    }
  }

  // Normalize leading-dot floats like ".5" or "-.5" for std::from_chars.
  std::string normalized;
  std::string_view float_sv = text;
  if (text.front() == '.') {
    normalized = "0";
    normalized.append(text);
    float_sv = normalized;
  } else if (text.size() >= 2 && text[0] == '-' && text[1] == '.') {
    normalized = "-0";
    normalized.append(text.substr(1));
    float_sv = normalized;
  }

  double dval = 0.0;
  const auto [ptr, ec] = std::from_chars(
      float_sv.data(), float_sv.data() + float_sv.size(), dval);
  if (ec == std::errc{} && ptr == float_sv.data() + float_sv.size() &&
      std::isfinite(dval)) {
    return jsoncons::json(dval);
  }

  return jsoncons::json(std::string(text));
}

} // namespace detail

/// Converts a scalar Token into a typed `jsoncons::json` value.
/// Quoted `TokenType::String` tokens always remain strings.
[[nodiscard]] inline jsoncons::json parse_scalar(const Token &token) {
  switch (token.type) {
  case TokenType::String:
    return jsoncons::json(detail::normalize_whitespace(token.text));
  case TokenType::Number:
    return detail::parse_number_literal(token.text);
  case TokenType::Identifier: {
    if (token.text == "true") {
      return jsoncons::json(true);
    }
    if (token.text == "false") {
      return jsoncons::json(false);
    }
    if (token.text == "null" || token.text == "undefined") {
      return jsoncons::json::null();
    }
    return jsoncons::json(detail::normalize_whitespace(token.text));
  }
  default:
    return jsoncons::json(detail::normalize_whitespace(token.text));
  }
}

} // namespace digger
