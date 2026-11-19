#include "common/json_utils.hpp"
#include "common/utf.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <sstream>
#include <string>

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
