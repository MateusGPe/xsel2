#pragma once

#include "common/utf.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xsel::common {

enum class FuzzyAlgo { JaroWinkler, Levenshtein };

[[nodiscard]] inline FuzzyAlgo parse_fuzzy_algo(std::string_view name) {
  if (name == "jaro_winkler" || name == "jw") {
    return FuzzyAlgo::JaroWinkler;
  }
  if (name == "levenshtein" || name == "lev" || name == "edit") {
    return FuzzyAlgo::Levenshtein;
  }
  throw std::invalid_argument("Unknown fuzzy algorithm: " + std::string(name));
}

[[nodiscard]] inline int32_t fold_ascii_cp(int32_t cp, bool case_sensitive) {
  if (!case_sensitive && cp >= 'A' && cp <= 'Z') {
    return cp + ('a' - 'A');
  }
  return cp;
}

[[nodiscard]] inline std::vector<int32_t>
decode_codepoints(std::string_view s, bool case_sensitive = true) {
  std::vector<int32_t> cps;
  cps.reserve(s.size());
  const char *ptr = s.data();
  size_t remaining = s.size();
  while (remaining > 0) {
    int32_t cp = 0;
    const char *next = utf8_iterate(ptr, remaining, &cp);
    if (!next || next == ptr) {
      cps.push_back(fold_ascii_cp(static_cast<unsigned char>(*ptr), case_sensitive));
      ++ptr;
      --remaining;
    } else {
      cps.push_back(fold_ascii_cp(cp, case_sensitive));
      remaining -= static_cast<size_t>(next - ptr);
      ptr = next;
    }
  }
  return cps;
}

namespace detail {

[[nodiscard]] inline size_t
levenshtein_distance_cps(std::vector<int32_t> ca, std::vector<int32_t> cb) {
  if (ca.size() < cb.size()) {
    std::swap(ca, cb);
  }
  const size_t n = ca.size();
  const size_t m = cb.size();
  if (m == 0) {
    return n;
  }

  std::vector<size_t> prev(m + 1);
  std::vector<size_t> curr(m + 1);
  for (size_t j = 0; j <= m; ++j) {
    prev[j] = j;
  }

  for (size_t i = 1; i <= n; ++i) {
    curr[0] = i;
    for (size_t j = 1; j <= m; ++j) {
      const size_t cost = (ca[i - 1] == cb[j - 1]) ? 0 : 1;
      curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
    }
    std::swap(prev, curr);
  }
  return prev[m];
}

} // namespace detail

[[nodiscard]] inline size_t
levenshtein_distance(std::string_view a, std::string_view b,
                     bool case_sensitive = false) {
  return detail::levenshtein_distance_cps(
      decode_codepoints(a, case_sensitive),
      decode_codepoints(b, case_sensitive));
}

[[nodiscard]] inline double
levenshtein_similarity(std::string_view a, std::string_view b,
                       bool case_sensitive = false) {
  auto ca = decode_codepoints(a, case_sensitive);
  auto cb = decode_codepoints(b, case_sensitive);
  const size_t max_len = std::max(ca.size(), cb.size());
  if (max_len == 0) {
    return 1.0;
  }
  const size_t dist =
      detail::levenshtein_distance_cps(std::move(ca), std::move(cb));
  return 1.0 - static_cast<double>(dist) / static_cast<double>(max_len);
}

[[nodiscard]] inline double
jaro_winkler_similarity(std::string_view a, std::string_view b,
                        bool case_sensitive = false) {
  const auto ca = decode_codepoints(a, case_sensitive);
  const auto cb = decode_codepoints(b, case_sensitive);
  const size_t len1 = ca.size();
  const size_t len2 = cb.size();
  if (len1 == 0 && len2 == 0) {
    return 1.0;
  }
  if (len1 == 0 || len2 == 0) {
    return 0.0;
  }

  const size_t max_dist =
      (std::max(len1, len2) / 2 > 0) ? (std::max(len1, len2) / 2 - 1) : 0;

  std::vector<bool> match1(len1, false);
  std::vector<bool> match2(len2, false);
  size_t matches = 0;

  for (size_t i = 0; i < len1; ++i) {
    const size_t start = (i > max_dist) ? (i - max_dist) : 0;
    const size_t end = std::min(i + max_dist + 1, len2);
    for (size_t j = start; j < end; ++j) {
      if (!match2[j] && ca[i] == cb[j]) {
        match1[i] = true;
        match2[j] = true;
        ++matches;
        break;
      }
    }
  }

  if (matches == 0) {
    return 0.0;
  }

  size_t transpositions = 0;
  size_t k = 0;
  for (size_t i = 0; i < len1; ++i) {
    if (!match1[i]) {
      continue;
    }
    while (!match2[k]) {
      ++k;
    }
    if (ca[i] != cb[k]) {
      ++transpositions;
    }
    ++k;
  }

  const double m = static_cast<double>(matches);
  const double jaro =
      (m / static_cast<double>(len1) + m / static_cast<double>(len2) +
       (m - static_cast<double>(transpositions) / 2.0) / m) /
      3.0;

  size_t prefix = 0;
  const size_t max_prefix = std::min<size_t>({4, len1, len2});
  for (size_t i = 0; i < max_prefix; ++i) {
    if (ca[i] == cb[i]) {
      ++prefix;
    } else {
      break;
    }
  }

  constexpr double kWinklerScale = 0.1;
  return jaro + static_cast<double>(prefix) * kWinklerScale * (1.0 - jaro);
}

[[nodiscard]] inline double
fuzzy_similarity(std::string_view a, std::string_view b, FuzzyAlgo algo,
                 bool case_sensitive = false) {
  switch (algo) {
  case FuzzyAlgo::JaroWinkler:
    return jaro_winkler_similarity(a, b, case_sensitive);
  case FuzzyAlgo::Levenshtein:
    return levenshtein_similarity(a, b, case_sensitive);
  }
  return 0.0;
}

} // namespace xsel::common
