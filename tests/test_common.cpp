#include "common/fuzzy.hpp"
#include "common/hex_utils.hpp"
#include "common/json_utils.hpp"
#include "common/string_utils.hpp"
#include "common/utf.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

TEST_CASE("utf8_encode and utf8_iterate round-trip codepoints", "[common][utf]") {
  const int32_t codepoints[] = {0x00, 0x41, 0x7F, 0x80, 0x00E9, 0x07FF,
                                0x0800, 0x20AC, 0xFFFF, 0x10000, 0x1F600,
                                0x10FFFF};
  for (int32_t cp : codepoints) {
    char buf[4]{};
    size_t len = 0;
    REQUIRE(utf8_encode(cp, buf, &len) == 0);
    REQUIRE(len >= 1);
    REQUIRE(len <= 4);
    REQUIRE(utf8_check_string(buf, len) == 1);

    int32_t decoded = -1;
    const char *next = utf8_iterate(buf, len, &decoded);
    REQUIRE(next == buf + len);
    CHECK(decoded == cp);
  }

  // Invalid codepoints
  char buf[4]{};
  size_t len = 0;
  CHECK(utf8_encode(-1, buf, &len) == -1);
  CHECK(utf8_encode(0x110000, buf, &len) == -1);
}

TEST_CASE("utf8_check_string rejects overlong, surrogate, and truncated UTF-8",
          "[common][utf]") {
  // Valid ASCII + multi-byte
  const std::string valid = "Hello \xE2\x82\xAC \xF0\x9F\x98\x80";
  CHECK(utf8_check_string(valid.data(), valid.size()) == 1);

  // Overlong 2-byte encoding of '/'
  const char overlong[] = {'\xC0', '\xAF'};
  CHECK(utf8_check_string(overlong, 2) == 0);

  // UTF-16 surrogate U+D800 encoded in UTF-8
  const char surrogate[] = {'\xED', '\xA0', '\x80'};
  CHECK(utf8_check_string(surrogate, 3) == 0);

  // Truncated 3-byte sequence
  const char truncated[] = {'\xE2', '\x82'};
  CHECK(utf8_check_string(truncated, 2) == 0);

  // Stray continuation byte
  const char continuation[] = {'\x80'};
  CHECK(utf8_check_string(continuation, 1) == 0);
}

TEST_CASE("dump_string escapes control chars, quotes, and non-ASCII",
          "[common][utf]") {
  const std::string input = "a\\b\"c\n\t\x01\xC3\xA9\xF0\x9F\x98\x80/";
  std::ostringstream oss;
  dump_string(input.data(), input.size(), oss);
  CHECK(oss.str() == R"(a\\b\"c\n\t\u0001\u00E9\uD83D\uDE00/)");
}

TEST_CASE("is_empty_json identifies empty containers, strings, and null",
          "[common][json_utils]") {
  CHECK(xsel::common::is_empty_json(jsoncons::json::null()));
  CHECK(xsel::common::is_empty_json(jsoncons::json("")));
  CHECK(xsel::common::is_empty_json(jsoncons::json::array()));
  CHECK(xsel::common::is_empty_json(jsoncons::json::object()));

  CHECK_FALSE(xsel::common::is_empty_json(jsoncons::json(false)));
  CHECK_FALSE(xsel::common::is_empty_json(jsoncons::json(0)));
  CHECK_FALSE(xsel::common::is_empty_json(jsoncons::json("x")));
  CHECK_FALSE(xsel::common::is_empty_json(jsoncons::json::parse("[1]")));
  CHECK_FALSE(xsel::common::is_empty_json(jsoncons::json::parse(R"({"a":1})")));
}

TEST_CASE("canonical_json_key is key-order invariant and enforces depth",
          "[common][json_utils]") {
  const auto a = jsoncons::json::parse(R"({"b": [1, {"y": 2, "x": "hi\n"}], "a": true})");
  const auto b = jsoncons::json::parse(R"({"a": true, "b": [1, {"x": "hi\n", "y": 2}]})");
  CHECK(xsel::common::canonical_json_key(a) ==
        xsel::common::canonical_json_key(b));

  // Depth guard
  jsoncons::json deep = 1;
  for (int i = 0; i < xsel::common::MAX_JSON_DEPTH + 5; ++i) {
    jsoncons::json wrap = jsoncons::json::array();
    wrap.push_back(std::move(deep));
    deep = std::move(wrap);
  }
  CHECK_THROWS_AS((void)xsel::common::canonical_json_key(deep),
                  std::runtime_error);
}

TEST_CASE("compare_json compares large integers without double precision loss",
          "[common][json_utils]") {
  const jsoncons::json i1(int64_t{9007199254740992LL}); // 2^53
  const jsoncons::json i2(int64_t{9007199254740993LL}); // 2^53 + 1
  CHECK(xsel::common::compare_json(i1, i2) < 0);
  CHECK(xsel::common::compare_json(i2, i1) > 0);
  CHECK(xsel::common::compare_json(i1, i1) == 0);

  const jsoncons::json umax(UINT64_MAX);
  const jsoncons::json umax_minus1(UINT64_MAX - 1ULL);
  CHECK(xsel::common::compare_json(umax_minus1, umax) < 0);
  CHECK(xsel::common::compare_json(umax, umax_minus1) > 0);

  const jsoncons::json neg(int64_t{-5});
  const jsoncons::json pos_u(uint64_t{5});
  CHECK(xsel::common::compare_json(neg, pos_u) < 0);
  CHECK(xsel::common::compare_json(pos_u, neg) > 0);

  const jsoncons::json s1("apple");
  const jsoncons::json s2("banana");
  CHECK(xsel::common::compare_json(s1, s2) < 0);
}

TEST_CASE("next_unique_key and json_to_string behave as expected",
          "[common][json_utils]") {
  jsoncons::json obj = jsoncons::json::parse(R"({"x": 1, "x_0": 2})");
  CHECK(xsel::common::next_unique_key(obj, "y") == "y");
  CHECK(xsel::common::next_unique_key(obj, "x") == "x_1");
  CHECK(xsel::common::next_unique_key(jsoncons::json::array(), "x") == "x");

  CHECK(xsel::common::json_to_string(jsoncons::json("abc")) == "abc");
  CHECK(xsel::common::json_to_string(jsoncons::json(42)) == "42");
  CHECK(xsel::common::json_to_string(jsoncons::json(true)) == "true");
  CHECK(xsel::common::json_to_string(jsoncons::json::null()) == "null");
}

TEST_CASE("hex_utils and check_json_depth handle valid and invalid inputs",
          "[common][hex]") {
  CHECK_NOTHROW(xsel::common::check_json_depth(xsel::common::MAX_JSON_DEPTH, "ok"));
  CHECK_THROWS_AS(
      xsel::common::check_json_depth(xsel::common::MAX_JSON_DEPTH + 1, "deep"),
      std::runtime_error);

  const uint8_t bytes[] = {0x00, 0x0f, 0xa5, 0xff};
  CHECK(xsel::common::bytes_to_hex(bytes) == "000fa5ff");
}

TEST_CASE("compare_json handles mixed int64/uint64/double and NaN ordering",
          "[common][json_utils][edge]") {
  const jsoncons::json big_i(int64_t{9007199254740993LL}); // 2^53 + 1
  const jsoncons::json dbl_2_53(9007199254740992.0);        // 2^53
  CHECK(xsel::common::compare_json(big_i, dbl_2_53) > 0);
  CHECK(xsel::common::compare_json(dbl_2_53, big_i) < 0);

  const jsoncons::json neg_inf(-std::numeric_limits<double>::infinity());
  const jsoncons::json pos_inf(std::numeric_limits<double>::infinity());
  const jsoncons::json nan_val(std::numeric_limits<double>::quiet_NaN());
  const jsoncons::json min_i(std::numeric_limits<int64_t>::min());
  const jsoncons::json max_u(std::numeric_limits<uint64_t>::max());

  CHECK(xsel::common::compare_json(neg_inf, min_i) < 0);
  CHECK(xsel::common::compare_json(min_i, neg_inf) > 0);
  CHECK(xsel::common::compare_json(max_u, pos_inf) < 0);
  CHECK(xsel::common::compare_json(pos_inf, max_u) > 0);

  // NaN obeys strict weak ordering (sorts after finite/infinite numbers)
  CHECK(xsel::common::compare_json(pos_inf, nan_val) < 0);
  CHECK(xsel::common::compare_json(nan_val, pos_inf) > 0);
  CHECK(xsel::common::compare_json(nan_val, nan_val) == 0);
}

TEST_CASE("append_utf8_codepoint replaces surrogates and out-of-range values",
          "[common][utf][edge]") {
  std::string out;
  xsel::common::append_utf8_codepoint(out, 0xD800u);
  xsel::common::append_utf8_codepoint(out, 0xDFFFu);
  xsel::common::append_utf8_codepoint(out, 0x110000u);
  CHECK(out == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
  CHECK(utf8_check_string(out.data(), out.size()) == 1);
}

TEST_CASE("utf8_check_string rejects 3-byte/4-byte overlong and >U+10FFFF",
          "[common][utf][edge]") {
  CHECK(utf8_check_string("", 0) == 1);
  const char overlong3[] = {'\xE0', '\x80', '\xAF'};
  CHECK(utf8_check_string(overlong3, 3) == 0);
  const char overlong4[] = {'\xF0', '\x80', '\x80', '\xAF'};
  CHECK(utf8_check_string(overlong4, 4) == 0);
  const char too_high[] = {'\xF4', '\x90', '\x80', '\x80'}; // U+110000
  CHECK(utf8_check_string(too_high, 4) == 0);
}

TEST_CASE("strip_c_comments handles strings, escapes, EOF, and preserve_newlines",
          "[common][string_utils][edge]") {
  CHECK(xsel::common::strip_c_comments(R"("http://x" // comment at eof)") ==
        R"("http://x" )");
  CHECK(xsel::common::strip_c_comments(R"('a/*b*/c' /* c1 */ 'd\"/*e*/')") ==
        R"('a/*b*/c'  'd\"/*e*/')");
  CHECK(xsel::common::strip_c_comments("a /* unterminated\nline2") == "a ");
  CHECK(xsel::common::strip_c_comments("a /* l1\nl2 */ b", true, true, true) ==
        "a \n b");
  CHECK(xsel::common::strip_c_comments("a /* l1\nl2 */ b", true, true, false) ==
        "a  b");
}

TEST_CASE("hex_nibble handles valid and boundary invalid characters",
          "[common][hex][edge]") {
  CHECK(xsel::common::hex_nibble('0') == 0);
  CHECK(xsel::common::hex_nibble('9') == 9);
  CHECK(xsel::common::hex_nibble('a') == 10);
  CHECK(xsel::common::hex_nibble('f') == 15);
  CHECK(xsel::common::hex_nibble('A') == 10);
  CHECK(xsel::common::hex_nibble('F') == 15);
  CHECK(xsel::common::hex_nibble('/') == -1);
  CHECK(xsel::common::hex_nibble(':') == -1);
  CHECK(xsel::common::hex_nibble('g') == -1);
  CHECK(xsel::common::hex_nibble('G') == -1);
  CHECK(xsel::common::hex_nibble('\0') == -1);
}

TEST_CASE("fuzzy matching handles empty, UTF-8, case sensitivity, and errors",
          "[common][fuzzy][edge]") {
  using namespace xsel::common;
  CHECK(parse_fuzzy_algo("jw") == FuzzyAlgo::JaroWinkler);
  CHECK(parse_fuzzy_algo("lev") == FuzzyAlgo::Levenshtein);
  CHECK_THROWS_AS(parse_fuzzy_algo("soundex"), std::invalid_argument);

  CHECK(levenshtein_distance("", "") == 0);
  CHECK(levenshtein_distance("", "abc") == 3);
  CHECK(levenshtein_similarity("", "") == 1.0);
  CHECK(jaro_winkler_similarity("", "") == 1.0);
  CHECK(jaro_winkler_similarity("abc", "") == 0.0);
  CHECK(jaro_winkler_similarity("abc", "xyz") == 0.0);

  CHECK(levenshtein_distance("AbC", "abc", false) == 0);
  CHECK(levenshtein_distance("AbC", "abc", true) == 2);

  // Multi-byte UTF-8 codepoints should be counted as 1 codepoint each, not bytes
  CHECK(levenshtein_distance("caf\xC3\xA9", "cafe") == 1);
}

TEST_CASE("compare_json handles negative zero, fractions, and exact int64/double boundaries",
          "[common][json_utils][edge]") {
  CHECK(xsel::common::compare_json(jsoncons::json(-0.0), jsoncons::json(0.0)) == 0);
  CHECK(xsel::common::compare_json(jsoncons::json(42), jsoncons::json(42.5)) < 0);
  CHECK(xsel::common::compare_json(jsoncons::json(-42), jsoncons::json(-42.5)) > 0);

  const jsoncons::json min_i(std::numeric_limits<int64_t>::min());
  const jsoncons::json min_i_dbl(-9223372036854775808.0);
  CHECK(xsel::common::compare_json(min_i, min_i_dbl) == 0);

  const jsoncons::json max_i(std::numeric_limits<int64_t>::max());
  const jsoncons::json pow2_63(9223372036854775808.0);
  CHECK(xsel::common::compare_json(max_i, pow2_63) < 0);
  CHECK(xsel::common::compare_json(pow2_63, max_i) > 0);
}

TEST_CASE("Hardening: parse_hex_4 overflow, utf8_encode surrogates, dump_string recovery, and compare_json total order",
          "[common][hardening]") {
  // parse_hex_4 must not wrap around when pos is near SIZE_MAX
  CHECK(xsel::common::parse_hex_4("1234", std::string_view::npos) == -1);
  CHECK(xsel::common::parse_hex_4("1234", std::numeric_limits<size_t>::max() - 1) == -1);

  // utf8_encode must reject UTF-16 surrogates
  char buf[4]{};
  size_t len = 0;
  CHECK(utf8_encode(0xD800, buf, &len) == -1);
  CHECK(utf8_encode(0xDFFF, buf, &len) == -1);

  // dump_string replaces invalid UTF-8 bytes with \uFFFD and continues
  const std::string bad_utf8 = "a\xFFz";
  std::ostringstream oss;
  dump_string(bad_utf8.data(), bad_utf8.size(), oss);
  CHECK(oss.str() == R"(a\uFFFDz)");

  // compare_json provides strict weak ordering across heterogeneous types
  std::vector<jsoncons::json> mixed = {
      jsoncons::json::parse(R"({"b": 1})"),
      jsoncons::json("2"),
      jsoncons::json(10),
      jsoncons::json::parse("[2]"),
      jsoncons::json(true),
      jsoncons::json::null(),
      jsoncons::json("10"),
      jsoncons::json(2),
      jsoncons::json(false),
      jsoncons::json::parse("[1]"),
      jsoncons::json::parse(R"({"a": 1})"),
  };
  std::sort(mixed.begin(), mixed.end(),
            [](const jsoncons::json &a, const jsoncons::json &b) {
              return xsel::common::compare_json(a, b) < 0;
            });

  REQUIRE(mixed.size() == 11);
  CHECK(mixed[0].is_null());
  CHECK(mixed[1] == jsoncons::json(false));
  CHECK(mixed[2] == jsoncons::json(true));
  CHECK(mixed[3] == jsoncons::json(2));
  CHECK(mixed[4] == jsoncons::json(10));
  CHECK(mixed[5] == jsoncons::json("10"));
  CHECK(mixed[6] == jsoncons::json("2"));
  CHECK(mixed[7] == jsoncons::json::parse("[1]"));
  CHECK(mixed[8] == jsoncons::json::parse("[2]"));
  CHECK(mixed[9] == jsoncons::json::parse(R"({"a": 1})"));
  CHECK(mixed[10] == jsoncons::json::parse(R"({"b": 1})"));
}
