#include "scraper/steps/text_steps.hpp"
#include "scraper/merge.hpp"
#include "common/fuzzy.hpp"
#include "common/hex_utils.hpp"
#include "common/string_utils.hpp"
#include "common/utf.hpp"
#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <lxml2-query.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utf8proc.h>

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
        if (next == 'b') {
          decoded += '\b';
          ++i;
          continue;
        }
        if (next == 'f') {
          decoded += '\f';
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

void StripTagsStep::apply(Node, std::vector<std::string> &values,
                          jsoncons::json &) {
  for (auto &v : values) {
    std::string out;
    out.reserve(v.size());
    bool in_tag = false;
    char quote = '\0';
    for (size_t i = 0; i < v.size(); ++i) {
      const char c = v[i];
      if (in_tag) {
        if (quote != '\0') {
          if (c == quote) {
            quote = '\0';
          }
        } else if (c == '"' || c == '\'') {
          quote = c;
        } else if (c == '>') {
          in_tag = false;
        }
      } else {
        if (c == '<') {
          in_tag = true;
          quote = '\0';
        } else {
          out.push_back(c);
        }
      }
    }
    v = std::move(out);
  }
}

void Base64EncodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &) {
  for (auto &v : values) {
    std::string encoded;
    if (urlsafe_) {
      absl::WebSafeBase64Escape(v, &encoded);
    } else {
      absl::Base64Escape(v, &encoded);
    }
    v = std::move(encoded);
  }
}

void Base64DecodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &) {
  for (auto &v : values) {
    std::string decoded;
    const bool ok = urlsafe_ ? absl::WebSafeBase64Unescape(v, &decoded)
                             : absl::Base64Unescape(v, &decoded);
    if (!ok) {
      throw std::runtime_error("base64_decode: invalid base64 input");
    }
    v = std::move(decoded);
  }
}

namespace {

uint64_t fnv1a_64(std::string_view data) noexcept {
  uint64_t h = 14695981039346656037ULL;
  for (unsigned char c : data) {
    h ^= static_cast<uint64_t>(c);
    h *= 1099511628211ULL;
  }
  return h;
}

std::array<uint8_t, 32> sha256_bytes(std::string_view data) {
  static constexpr std::array<uint32_t, 64> K = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  auto rotr = [](uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32u - n));
  };

  std::array<uint32_t, 8> H = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                               0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                               0x1f83d9abu, 0x5be0cd19u};

  const uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8ULL;
  std::vector<uint8_t> msg(data.begin(), data.end());
  msg.push_back(0x80u);
  while ((msg.size() % 64u) != 56u) {
    msg.push_back(0x00u);
  }
  for (int i = 7; i >= 0; --i) {
    msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFu));
  }

  for (size_t offset = 0; offset < msg.size(); offset += 64u) {
    std::array<uint32_t, 64> W{};
    for (size_t t = 0; t < 16u; ++t) {
      const size_t j = offset + t * 4u;
      W[t] = (static_cast<uint32_t>(msg[j]) << 24) |
             (static_cast<uint32_t>(msg[j + 1]) << 16) |
             (static_cast<uint32_t>(msg[j + 2]) << 8) |
             static_cast<uint32_t>(msg[j + 3]);
    }
    for (size_t t = 16u; t < 64u; ++t) {
      const uint32_t s0 = rotr(W[t - 15], 7) ^ rotr(W[t - 15], 18) ^ (W[t - 15] >> 3);
      const uint32_t s1 = rotr(W[t - 2], 17) ^ rotr(W[t - 2], 19) ^ (W[t - 2] >> 10);
      W[t] = W[t - 16] + s0 + W[t - 7] + s1;
    }
    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];
    for (size_t t = 0; t < 64u; ++t) {
      const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t ch = (e & f) ^ ((~e) & g);
      const uint32_t temp1 = h + S1 + ch + K[t] + W[t];
      const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = S0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
    H[5] += f;
    H[6] += g;
    H[7] += h;
  }

  std::array<uint8_t, 32> digest{};
  for (size_t i = 0; i < 8u; ++i) {
    digest[i * 4 + 0] = static_cast<uint8_t>((H[i] >> 24) & 0xFFu);
    digest[i * 4 + 1] = static_cast<uint8_t>((H[i] >> 16) & 0xFFu);
    digest[i * 4 + 2] = static_cast<uint8_t>((H[i] >> 8) & 0xFFu);
    digest[i * 4 + 3] = static_cast<uint8_t>(H[i] & 0xFFu);
  }
  return digest;
}

} // namespace

HashStep::HashStep(std::string_view algo) {
  const std::string lower = absl::AsciiStrToLower(algo);
  if (lower == "fnv1a64" || lower == "fnv1a") {
    algo_ = Algo::Fnv1a64;
  } else if (lower == "sha256" || lower == "sha-256") {
    algo_ = Algo::Sha256;
  } else {
    throw std::runtime_error("hash: unknown algorithm '" + std::string(algo) +
                             "' (expected 'fnv1a64' or 'sha256')");
  }
}

void HashStep::apply(Node, std::vector<std::string> &values,
                     jsoncons::json &) {
  for (auto &v : values) {
    if (algo_ == Algo::Fnv1a64) {
      const uint64_t h = fnv1a_64(v);
      std::array<uint8_t, 8> bytes{};
      for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<uint8_t>((h >> ((7 - i) * 8)) & 0xFFu);
      }
      v = xsel::common::bytes_to_hex(bytes);
    } else {
      const auto digest = sha256_bytes(v);
      v = xsel::common::bytes_to_hex(digest);
    }
  }
}

namespace {

std::string run_utf8proc(std::string_view input, utf8proc_option_t options) {
  if (input.empty()) {
    return {};
  }
  utf8proc_uint8_t *mapped = nullptr;
  const utf8proc_ssize_t len = utf8proc_map(
      reinterpret_cast<const utf8proc_uint8_t *>(input.data()),
      static_cast<utf8proc_ssize_t>(input.size()), &mapped, options);
  std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> guard(mapped,
                                                                &std::free);
  if (len < 0 || !mapped) {
    throw std::runtime_error(std::string("unicode_normalize: ") +
                             utf8proc_errmsg(len));
  }
  return std::string(reinterpret_cast<const char *>(mapped),
                     static_cast<size_t>(len));
}

} // namespace

UnicodeNormalizeStep::UnicodeNormalizeStep(std::string_view form) {
  const std::string upper = absl::AsciiStrToUpper(form);
  if (upper == "NFC") {
    form_ = Form::NFC;
  } else if (upper == "NFD") {
    form_ = Form::NFD;
  } else if (upper == "NFKC") {
    form_ = Form::NFKC;
  } else if (upper == "NFKD") {
    form_ = Form::NFKD;
  } else {
    throw std::invalid_argument("unicode_normalize: unknown form '" +
                                std::string(form) +
                                "' (expected NFC, NFD, NFKC, or NFKD)");
  }
}

void UnicodeNormalizeStep::apply(Node, std::vector<std::string> &values,
                                 jsoncons::json &) {
  utf8proc_option_t opts = UTF8PROC_STABLE;
  switch (form_) {
  case Form::NFC:
    opts = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE);
    break;
  case Form::NFD:
    opts = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_DECOMPOSE);
    break;
  case Form::NFKC:
    opts = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPAT |
                                          UTF8PROC_COMPOSE);
    break;
  case Form::NFKD:
    opts = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPAT |
                                          UTF8PROC_DECOMPOSE);
    break;
  }
  for (auto &v : values) {
    v = run_utf8proc(v, opts);
  }
}

void StripAccentsStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &) {
  const auto opts = static_cast<utf8proc_option_t>(
      UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK);
  for (auto &v : values) {
    v = run_utf8proc(v, opts);
  }
}

FuzzyFilterStep::FuzzyFilterStep(std::string_view query, double threshold,
                                 std::string_view algo, bool case_sensitive,
                                 bool sort_by_score)
    : query_(query), threshold_(threshold), algo_(algo),
      case_sensitive_(case_sensitive), sort_by_score_(sort_by_score) {
  (void)xsel::common::parse_fuzzy_algo(algo_);
}

void FuzzyFilterStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &) {
  const auto algo = xsel::common::parse_fuzzy_algo(algo_);
  struct Scored {
    std::string value;
    double score;
  };
  std::vector<Scored> kept;
  kept.reserve(values.size());
  for (auto &v : values) {
    const double score =
        xsel::common::fuzzy_similarity(v, query_, algo, case_sensitive_);
    if (score >= threshold_) {
      kept.push_back({std::move(v), score});
    }
  }
  if (sort_by_score_) {
    std::stable_sort(kept.begin(), kept.end(),
                     [](const Scored &a, const Scored &b) {
                       return a.score > b.score;
                     });
  }
  values.clear();
  values.reserve(kept.size());
  for (auto &item : kept) {
    values.push_back(std::move(item.value));
  }
}

SimilarityStep::SimilarityStep(std::string_view query, std::string_view algo,
                               bool case_sensitive, std::string_view key)
    : query_(query), algo_(algo), case_sensitive_(case_sensitive), key_(key) {
  (void)xsel::common::parse_fuzzy_algo(algo_);
}

void SimilarityStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &dict) {
  const auto algo = xsel::common::parse_fuzzy_algo(algo_);
  std::vector<double> scores;
  scores.reserve(values.size());
  for (auto &v : values) {
    const double score =
        xsel::common::fuzzy_similarity(v, query_, algo, case_sensitive_);
    scores.push_back(score);
    std::ostringstream oss;
    oss << score;
    v = oss.str();
  }
  if (!key_.empty() && !scores.empty()) {
    if (scores.size() == 1) {
      insert_into_json(dict, key_, jsoncons::json(scores.front()));
    } else {
      jsoncons::json arr = jsoncons::json::array();
      for (double s : scores) {
        arr.push_back(s);
      }
      insert_into_json(dict, key_, std::move(arr));
    }
  }
}

} // namespace scraper
