#include "digger/digger.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

TEST_CASE("digger parses typed scalars from unquoted tokens", "[digger]") {
  const auto result = digger::parse_object(R"({
    count: 42,
    neg: -7,
    big: 9223372036854775807,
    ubig: 18446744073709551615,
    pi: 3.14159,
    neg_float: -0.5,
    leading_dot: .25,
    sci_pos: 1e+5,
    sci_neg: 2.5e-2,
    ok: true,
    disabled: false,
    empty: null,
    missing: undefined,
    quoted_num: "42",
    quoted_bool: 'true',
    label: "hello world"
  })");

  REQUIRE(result.is_object());
  CHECK(result["count"].is_int64());
  CHECK(result["count"].as<int64_t>() == 42);
  CHECK(result["neg"].as<int64_t>() == -7);
  CHECK(result["big"].as<int64_t>() == INT64_MAX);
  CHECK(result["ubig"].is_uint64());
  CHECK(result["ubig"].as<uint64_t>() == UINT64_MAX);

  CHECK(result["pi"].is_double());
  CHECK(result["pi"].as<double>() == Catch::Approx(3.14159));
  CHECK(result["neg_float"].as<double>() == Catch::Approx(-0.5));
  CHECK(result["leading_dot"].as<double>() == Catch::Approx(0.25));
  CHECK(result["sci_pos"].as<double>() == Catch::Approx(100000.0));
  CHECK(result["sci_neg"].as<double>() == Catch::Approx(0.025));

  CHECK(result["ok"].is_bool());
  CHECK(result["ok"].as<bool>() == true);
  CHECK(result["disabled"].is_bool());
  CHECK(result["disabled"].as<bool>() == false);

  CHECK(result["empty"].is_null());
  CHECK(result["missing"].is_null());

  CHECK(result["quoted_num"].is_string());
  CHECK(result["quoted_num"].as<std::string>() == "42");
  CHECK(result["quoted_bool"].is_string());
  CHECK(result["quoted_bool"].as<std::string>() == "true");
  CHECK(result["label"].as<std::string>() == "hello world");
}

TEST_CASE("digger handles JS comments, string concat, and JSON.parse", "[digger]") {
  const auto result = digger::parse_object(R"(
    const cfg = {
      // single-line comment
      title: "Hello " + "World",
      /* multi-line
         comment */
      nested: JSON.parse('{"items": [1, 2.5, true, null]}'),
      wrapped: ("ok")
    };
  )", true);

  REQUIRE(result.is_object());
  REQUIRE(result.contains("cfg"));
  const auto &cfg = result["cfg"];
  CHECK(cfg["title"].as<std::string>() == "Hello World");
  CHECK(cfg["wrapped"].as<std::string>() == "ok");
  REQUIRE(cfg["nested"]["items"].is_array());
  CHECK(cfg["nested"]["items"][0].as<int64_t>() == 1);
  CHECK(cfg["nested"]["items"][1].as<double>() == Catch::Approx(2.5));
  CHECK(cfg["nested"]["items"][2].as<bool>() == true);
  CHECK(cfg["nested"]["items"][3].is_null());
}

TEST_CASE("digger enforces MAX_PARSE_DEPTH without stack overflow", "[digger]") {
  std::string deep;
  deep.reserve(4000);
  for (int i = 0; i < 700; ++i) {
    deep += "{a:";
  }
  deep += "1";
  for (int i = 0; i < 700; ++i) {
    deep += "}";
  }
  const auto result = digger::parse_object(deep, true);
  REQUIRE(result.is_object());
}

TEST_CASE("digger TokenParser decodes escapes, template literals, and keywords",
          "[digger]") {
  const auto result = digger::parse_object(R"(
    let data = {
      hex: "\x41\x42",
      uni: "\u00E9",
      emoji: "\uD83D\uDE00",
      braced: "\u{1F680}",
      tmpl: `line1\nline2`,
      async: true,
      nested: [1, 2, ()]
    };
  )", true);

  REQUIRE(result.is_object());
  REQUIRE(result.contains("data"));
  const auto &d = result["data"];
  CHECK(d["hex"].as<std::string>() == "AB");
  CHECK(d["uni"].as<std::string>() == "\xC3\xA9");
  CHECK(d["emoji"].as<std::string>() == "\xF0\x9F\x98\x80");
  CHECK(d["braced"].as<std::string>() == "\xF0\x9F\x9A\x80");
  CHECK(d["tmpl"].as<std::string>() == "line1 line2");
  CHECK(d["nested"].is_array());
  CHECK(d["nested"].size() == 2);
}

TEST_CASE("digger parse_object_into merges into existing container",
          "[digger]") {
  jsoncons::json out = jsoncons::json::object();
  out["existing"] = "keep";
  digger::parse_object_into(R"({added: 99})", out, true);
  CHECK(out["existing"].as<std::string>() == "keep");
  CHECK(out["added"].as<int64_t>() == 99);
}
