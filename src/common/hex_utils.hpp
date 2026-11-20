#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace xsel::common {

/// Converts a single ASCII hex digit to 0..15, or -1 if invalid.
[[nodiscard]] constexpr int hex_nibble(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

/// Parses 4 hex digits starting at `pos` in `s`. Returns -1 if out of bounds or invalid.
[[nodiscard]] constexpr int parse_hex_4(std::string_view s, size_t pos) noexcept {
  if (pos > s.size() || s.size() - pos < 4) {
    return -1;
  }
  int val = 0;
  for (size_t i = 0; i < 4; ++i) {
    const int d = hex_nibble(s[pos + i]);
    if (d < 0) {
      return -1;
    }
    val = (val << 4) | d;
  }
  return val;
}

/// Encodes a byte span as lowercase hexadecimal.
[[nodiscard]] inline std::string bytes_to_hex(std::span<const uint8_t> bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (uint8_t b : bytes) {
    out.push_back(kHex[(b >> 4) & 0x0F]);
    out.push_back(kHex[b & 0x0F]);
  }
  return out;
}

} // namespace xsel::common
