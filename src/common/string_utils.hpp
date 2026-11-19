#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace xsel::common {

/// Strips C/JS-style `//` and `/* ... */` comments while preserving string
/// literals (`"..."` and `'...'`). When `preserve_newlines` is true, newlines
/// inside stripped comments are retained so line numbers remain accurate.
[[nodiscard]] inline std::string strip_c_comments(std::string_view src,
                                                  bool multiline = true,
                                                  bool single_line = true,
                                                  bool preserve_newlines = false) {
  std::string out;
  out.reserve(src.size());
  char quote = '\0';
  for (size_t i = 0; i < src.size(); ++i) {
    const char c = src[i];
    if (quote != '\0') {
      out.push_back(c);
      if (c == '\\' && i + 1 < src.size()) {
        out.push_back(src[++i]);
      } else if (c == quote) {
        quote = '\0';
      }
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      out.push_back(c);
    } else if (c == '/' && i + 1 < src.size()) {
      if (single_line && src[i + 1] == '/') {
        i += 2;
        while (i < src.size() && src[i] != '\n') {
          ++i;
        }
        if (i < src.size() && src[i] == '\n') {
          if (preserve_newlines) {
            out.push_back('\n');
          } else {
            --i; // Let outer loop or next iteration handle '\n' if needed
          }
        }
      } else if (multiline && src[i + 1] == '*') {
        i += 2;
        while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
          if (preserve_newlines && src[i] == '\n') {
            out.push_back('\n');
          }
          ++i;
        }
        if (i + 1 < src.size()) {
          ++i;
        } else {
          break;
        }
      } else {
        out.push_back(c);
      }
    } else {
      out.push_back(c);
    }
  }
  return out;
}

} // namespace xsel::common
