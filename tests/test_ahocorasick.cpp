#include <catch2/catch_test_macros.hpp>

#include "common/ahocorasick.hpp"

#include <string>

TEST_CASE("AhoCorasick basic AddPattern, Find, and FindAny", "[ahocorasick]") {
  AhoCorasick ac;
  REQUIRE(ac.IsEmpty());
  REQUIRE_FALSE(ac.IsCompiled());

  const uint32_t id_he = ac.AddPattern("he");
  const uint32_t id_she = ac.AddPattern("she");
  const uint32_t id_his = ac.AddPattern("his");
  const uint32_t id_hers = ac.AddPattern("hers");

  REQUIRE(id_he == 0u);
  REQUIRE(id_she == 1u);
  REQUIRE(id_his == 2u);
  REQUIRE(id_hers == 3u);

  // Duplicate pattern returns original ID, empty pattern is ignored.
  REQUIRE(ac.AddPattern("she") == id_she);
  REQUIRE(ac.AddPattern("") == AhoCorasick::kInvalidPattern);
  REQUIRE(ac.PatternCount() == 4u);

  ac.Compile();
  REQUIRE(ac.IsCompiled());

  AhoCorasick::MatchList matches;
  REQUIRE(ac.Find("ushers", matches));
  REQUIRE(matches.size() == 3u);

  // "she" at [1, 4), "he" at [2, 4), "hers" at [2, 6)
  REQUIRE(matches[0].start == 1u);
  REQUIRE(matches[0].length == 3u);
  REQUIRE(matches[0].pattern_id == id_she);

  REQUIRE(matches[1].start == 2u);
  REQUIRE(matches[1].length == 2u);
  REQUIRE(matches[1].pattern_id == id_he);

  REQUIRE(matches[2].start == 2u);
  REQUIRE(matches[2].length == 4u);
  REQUIRE(matches[2].pattern_id == id_hers);

  AhoCorasick::MatchList any_matches;
  REQUIRE(ac.FindAny("ushers", any_matches));
  REQUIRE(any_matches.size() == 2u);
  REQUIRE(any_matches[0].pattern_id == id_she);
  REQUIRE(any_matches[1].pattern_id == id_hers);
}

TEST_CASE("AhoCorasick HasAnyPattern and HasAllPatterns", "[ahocorasick]") {
  AhoCorasick ac;
  ac.Compile();
  REQUIRE(ac.IsEmpty());
  REQUIRE_FALSE(ac.HasAnyPattern("anything"));
  REQUIRE(ac.HasAllPatterns("anything"));

  ac.Clear();
  REQUIRE_FALSE(ac.IsCompiled());
  ac.AddPattern("alpha");
  ac.AddPattern("beta");
  ac.AddPattern("gamma");
  ac.Compile();

  REQUIRE(ac.HasAnyPattern("xxbetayy"));
  REQUIRE_FALSE(ac.HasAnyPattern("xxdeltayy"));
  REQUIRE(ac.HasAllPatterns("gamma-alpha-beta"));
  REQUIRE_FALSE(ac.HasAllPatterns("gamma-alpha"));
}

TEST_CASE("AhoCorasick HasAllPatterns bitmask boundary and >64 fallback",
          "[ahocorasick]") {
  // Exact 64-pattern boundary
  {
    AhoCorasick ac64;
    std::string haystack;
    for (uint32_t i = 0; i < 64u; ++i) {
      std::string pat = "p_" + std::to_string(i) + "_";
      REQUIRE(ac64.AddPattern(pat) == i);
      haystack += pat;
    }
    ac64.Compile();
    REQUIRE(ac64.HasAllPatterns(haystack));
    REQUIRE_FALSE(ac64.HasAllPatterns("p_0_p_1_"));
  }

  // > 64 patterns fallback path
  {
    AhoCorasick ac80;
    std::string haystack;
    for (uint32_t i = 0; i < 80u; ++i) {
      std::string pat = "k_" + std::to_string(i) + "_";
      REQUIRE(ac80.AddPattern(pat) == i);
      haystack += pat;
    }
    ac80.Compile();
    REQUIRE(ac80.HasAllPatterns(haystack));
    REQUIRE_FALSE(ac80.HasAllPatterns(haystack.substr(0, haystack.size() - 5)));
  }
}

TEST_CASE("AhoCorasick handles full 256-byte alphabet without overflow",
          "[ahocorasick]") {
  AhoCorasick ac;
  for (uint32_t b = 0; b < 256u; ++b) {
    std::string pat(1, static_cast<char>(b));
    pat.push_back('!');
    ac.AddPattern(pat);
  }
  ac.Compile();
  REQUIRE(ac.AlphabetSize() == 257u);

  std::string probe;
  probe.push_back(static_cast<char>(0));
  probe.push_back('!');
  probe.push_back(static_cast<char>(255));
  probe.push_back('!');

  AhoCorasick::MatchList matches;
  REQUIRE(ac.Find(probe, matches));
  REQUIRE(matches.size() == 2u);
  REQUIRE(matches[0].pattern_id == 0u);
  REQUIRE(matches[1].pattern_id == 255u);
}

TEST_CASE("AhoCorasick handles very long pattern without stack overflow",
          "[ahocorasick]") {
  AhoCorasick ac;
  const std::string long_pat(20000u, 'x');
  ac.AddPattern(long_pat);
  ac.Compile();

  REQUIRE(ac.HasAnyPattern(long_pat));
  REQUIRE_FALSE(ac.HasAnyPattern(std::string(19999u, 'x')));
}
