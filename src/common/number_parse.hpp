#pragma once

#include <charconv>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && \
    __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 130300
#define XSEL_USE_STRTOD_DOUBLE_FALLBACK 1
#include <cerrno>
#include <cstdlib>
#include <xlocale.h>
#else
#define XSEL_USE_STRTOD_DOUBLE_FALLBACK 0
#endif

namespace xsel::common {

/// Parses a strict finite decimal/scientific `double` matching `std::from_chars`
/// semantics (no leading whitespace, no leading `+`, no hex floats, full match).
/// Uses `strtod_l` with the C locale on older macOS deployment targets where
/// `std::from_chars(..., double)` is unavailable in Apple libc++.
[[nodiscard]] inline bool parse_double(std::string_view sv, double &out) noexcept {
  if (sv.empty()) {
    return false;
  }

#if !XSEL_USE_STRTOD_DOUBLE_FALLBACK
  double val = 0.0;
  const auto [ptr, ec] =
      std::from_chars(sv.data(), sv.data() + sv.size(), val);
  if (ec != std::errc{} || ptr != sv.data() + sv.size() || !std::isfinite(val)) {
    return false;
  }
  out = val;
  return true;
#else
  const unsigned char first = static_cast<unsigned char>(sv.front());
  if (first <= ' ' || first == '+') {
    return false;
  }
  // Reject hex floats (0x / 0X / -0x / -0X) which strtod_l accepts but
  // std::from_chars(chars_format::general) does not.
  const size_t prefix_pos = (sv.front() == '-') ? 1 : 0;
  if (sv.size() >= prefix_pos + 2 && sv[prefix_pos] == '0' &&
      (sv[prefix_pos + 1] == 'x' || sv[prefix_pos + 1] == 'X')) {
    return false;
  }

  char stack_buf[64];
  std::string heap_buf;
  const char *cstr = nullptr;
  if (sv.size() < sizeof(stack_buf)) {
    std::memcpy(stack_buf, sv.data(), sv.size());
    stack_buf[sv.size()] = '\0';
    cstr = stack_buf;
  } else {
    heap_buf.assign(sv);
    cstr = heap_buf.c_str();
  }

  errno = 0;
  char *end = nullptr;
  const double val = strtod_l(cstr, &end, nullptr);
  if (errno == ERANGE || end != cstr + sv.size() || !std::isfinite(val)) {
    return false;
  }
  out = val;
  return true;
#endif
}

} // namespace xsel::common
