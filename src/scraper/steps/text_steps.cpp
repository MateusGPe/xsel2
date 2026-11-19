#include "scraper/steps/text_steps.hpp"
#include "common/hex_utils.hpp"
#include "common/string_utils.hpp"
#include "common/utf.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <lxml2-query.h>
#include <stdexcept>

namespace scraper {

namespace detail {

void trim_in_place(std::string &value) {
  constexpr std::string_view kWhitespace = " \t\n\r\f\v";
  const auto str_begin = value.find_first_not_of(kWhitespace);
  if (str_begin == std::string::npos) {
    value.clear();
    return;
  }
  const auto str_end = value.find_last_not_of(kWhitespace);
  value.erase(str_end + 1);
  if (str_begin > 0) {
    value.erase(0, str_begin);
  }
}

std::vector<std::string> split_string(std::string_view s,
                                      std::string_view delimiter,
                                      bool skip_empty_parts) {
  std::vector<std::string> result;
  if (delimiter.empty()) {
    if (!s.empty() || !skip_empty_parts) {
      result.emplace_back(s);
    }
    return result;
  }
  size_t pos = 0;
  while (pos <= s.size()) {
    const size_t next = s.find(delimiter, pos);
    const size_t end = (next == std::string_view::npos) ? s.size() : next;
    std::string_view part = s.substr(pos, end - pos);
    if (!skip_empty_parts || !part.empty()) {
      result.emplace_back(part);
    }
    if (next == std::string_view::npos) {
      break;
    }
    pos = next + delimiter.size();
  }
  return result;
}

std::string url_unescape(std::string_view escaped_url) {
  std::string out;
  out.reserve(escaped_url.size());
  for (size_t i = 0; i < escaped_url.size(); ++i) {
    if (escaped_url[i] == '%' && i + 2 < escaped_url.size()) {
      const int hi = xsel::common::hex_nibble(escaped_url[i + 1]);
      const int lo = xsel::common::hex_nibble(escaped_url[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(escaped_url[i]);
  }
  return out;
}

} // namespace detail

void TrimStep::apply(Node, std::vector<std::string> &values, jsoncons::json &) {
  for (auto &v : values) {
    detail::trim_in_place(v);
  }
}

void StripStep::apply(Node, std::vector<std::string> &values, jsoncons::json &) {
  for (auto &v : values) {
    v = normalizeHtmlText(v);
  }
}

void LowerStep::apply(Node, std::vector<std::string> &values, jsoncons::json &) {
  for (auto &v : values) {
    v = toLower(v);
  }
}

void UpperStep::apply(Node, std::vector<std::string> &values, jsoncons::json &) {
  for (auto &v : values) {
    v = toUpper(v);
  }
}

void ReplaceStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &) {
  if (from_str_.empty())
    return;
  for (auto &v : values) {
    const size_t pos = v.find(from_str_);
    if (pos != std::string::npos) {
      v.replace(pos, from_str_.size(), to_str_);
    }
  }
}

void SubstituteStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  if (from_str_.empty())
    return;
  for (auto &v : values) {
    size_t pos = 0;
    while ((pos = v.find(from_str_, pos)) != std::string::npos) {
      v.replace(pos, from_str_.size(), to_str_);
      pos += to_str_.size();
    }
  }
}

SplitStep::SplitStep(std::string_view delimiter, bool keep_empty_parts)
    : delimiter_(delimiter), skip_empty_parts_(!keep_empty_parts) {
  if (delimiter_.empty()) {
    throw std::runtime_error("split: delimiter cannot be empty.");
  }
}

void SplitStep::apply(Node, std::vector<std::string> &values,
                      jsoncons::json &) {
  std::vector<std::string> out;
  for (const auto &v : values) {
    auto parts = detail::split_string(v, delimiter_, skip_empty_parts_);
    out.insert(out.end(), std::make_move_iterator(parts.begin()),
               std::make_move_iterator(parts.end()));
  }
  values = std::move(out);
}

void JoinStep::apply(Node, std::vector<std::string> &values, jsoncons::json &) {
  if (values.empty())
    return;
  size_t total_len = delimiter_.size() * (values.size() - 1);
  for (const auto &v : values) {
    total_len += v.size();
  }
  std::string joined;
  joined.reserve(total_len);
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0)
      joined += delimiter_;
    joined += values[i];
  }
  values.clear();
  values.push_back(std::move(joined));
}

void PrependStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &) {
  for (auto &v : values) {
    v.insert(0, prefix_str_);
  }
}

void AppendStep::apply(Node, std::vector<std::string> &values,
                       jsoncons::json &) {
  for (auto &v : values) {
    v.append(suffix_str_);
  }
}

void RemoveEmptyStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &) {
  values.erase(std::remove_if(values.begin(), values.end(),
                              [this](std::string &s) {
                                if (also_trim_) {
                                  detail::trim_in_place(s);
                                }
                                return s.empty();
                              }),
               values.end());
}

void SubstringStep::apply(Node, std::vector<std::string> &values,
                          jsoncons::json &) {
  for (auto &v : values) {
    const size_t n = v.size();
    size_t start = 0;
    if (start_pos_ >= 0) {
      start = static_cast<size_t>(start_pos_);
    } else {
      const size_t offset = static_cast<size_t>(-static_cast<int64_t>(start_pos_));
      start = (offset >= n) ? 0 : (n - offset);
    }
    if (start >= n) {
      v.clear();
      continue;
    }
    size_t count = n - start;
    if (length_.has_value()) {
      if (*length_ == 0) {
        v.clear();
        continue;
      }
      if (*length_ > 0) {
        count = std::min(static_cast<size_t>(*length_), n - start);
      } else {
        const size_t trim_end =
            static_cast<size_t>(-static_cast<int64_t>(*length_));
        const size_t end = (trim_end >= n) ? 0 : (n - trim_end);
        count = (end > start) ? (end - start) : 0;
      }
    }
    v = v.substr(start, count);
  }
}

void UnescapeStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &) {
  for (auto &v : values) {
    std::string decoded;
    decoded.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] == '\\' && i + 1 < v.size()) {
        const char next = v[i + 1];
        if (next == 'n') {
          decoded += '\n';
          ++i;
          continue;
        }
        if (next == 'r') {
          decoded += '\r';
          ++i;
          continue;
        }
        if (next == 't') {
          decoded += '\t';
          ++i;
          continue;
        }
        if (next == '\\' || next == '"' || next == '\'' || next == '/') {
          decoded += next;
          ++i;
          continue;
        }
        if (next == 'u' && i + 5 < v.size()) {
          const int cp = xsel::common::parse_hex_4(v, i + 2);
          if (cp >= 0) {
            uint32_t codepoint = static_cast<uint32_t>(cp);
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
                i + 11 < v.size() && v[i + 6] == '\\' && v[i + 7] == 'u') {
              const int low = xsel::common::parse_hex_4(v, i + 8);
              if (low >= 0xDC00 && low <= 0xDFFF) {
                codepoint =
                    0x10000 + (((codepoint - 0xD800) << 10) |
                               (static_cast<uint32_t>(low) - 0xDC00));
                i += 6;
              }
            }
            xsel::common::append_utf8_codepoint(decoded, codepoint);
            i += 5;
            continue;
          }
        }
      }
      decoded += v[i];
    }
    v = url_decode_ ? detail::url_unescape(decoded) : std::move(decoded);
  }
}

void StripCommentsStep::apply(Node, std::vector<std::string> &values,
                              jsoncons::json &) {
  for (auto &v : values) {
    v = xsel::common::strip_c_comments(v, multiline_, single_line_, false);
  }
}

} // namespace scraper
