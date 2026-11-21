#include "query/json_query.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("json_query evaluates JMESPath expressions", "[query]") {
  const auto doc = jsoncons::json::parse(R"({
    "inventory": [
      {"id": "A1", "name": "Desk Lamp", "price": 24.99, "available": true},
      {"id": "A2", "name": "Office Chair", "price": 149.00, "available": false},
      {"id": "A3", "name": "USB Cable", "price": 8.50, "available": true}
    ]
  })");

  const auto res = xsel::query::evaluate_jmespath(
      doc, "inventory[?available && price < `50`].{id: id, name: name}");
  REQUIRE(res.is_array());
  REQUIRE(res.size() == 2);
  CHECK(res[0]["id"].as<std::string>() == "A1");
  CHECK(res[1]["name"].as<std::string>() == "USB Cable");
}

TEST_CASE("json_query evaluates JSONPath expressions", "[query]") {
  const auto doc = jsoncons::json::parse(R"({
    "store": {
      "books": [
        {"title": "Book A", "price": 10},
        {"title": "Book B", "price": 25}
      ]
    }
  })");

  const auto res =
      xsel::query::evaluate_jsonpath(doc, "$.store.books[?(@.price > 15)].title");
  REQUIRE(res.is_array());
  REQUIRE(res.size() == 1);
  CHECK(res[0].as<std::string>() == "Book B");
}

TEST_CASE("json_query throws runtime_error on invalid expressions", "[query]") {
  const auto doc = jsoncons::json::parse(R"({"a": 1})");
  CHECK_THROWS_AS(xsel::query::evaluate_jmespath(doc, "a[?"),
                  std::runtime_error);
  CHECK_THROWS_AS(xsel::query::evaluate_jsonpath(doc, "$[invalid"),
                  std::runtime_error);
}

TEST_CASE("json_query evaluate dispatches dialects and handles missing paths",
          "[query]") {
  const auto doc = jsoncons::json::parse(R"({"user": {"name": "Ada"}})");

  const auto jmes = xsel::query::evaluate(
      xsel::query::QueryDialect::JmesPath, doc, "user.name");
  CHECK(jmes.as<std::string>() == "Ada");

  const auto jmes_missing = xsel::query::evaluate(
      xsel::query::QueryDialect::JmesPath, doc, "user.missing");
  CHECK(jmes_missing.is_null());

  const auto jp = xsel::query::evaluate(
      xsel::query::QueryDialect::JsonPath, doc, "$.user.name");
  REQUIRE(jp.is_array());
  REQUIRE(jp.size() == 1);
  CHECK(jp[0].as<std::string>() == "Ada");

  const auto jp_empty = xsel::query::evaluate(
      xsel::query::QueryDialect::JsonPath, jsoncons::json::object(), "$.a");
  REQUIRE(jp_empty.is_array());
  CHECK(jp_empty.empty());
}


TEST_CASE("json_query parse_json and dump_json handle options", "[query]") {
  const std::string raw = R"({
    // line comment
    "a": 1, /* block comment */
    "b": [1, 2,],
    "c": 123.45
  })";

  CHECK_THROWS_AS(xsel::query::parse_json(raw), std::runtime_error);

  xsel::query::JsonParseOptions opts;
  opts.allow_comments = true;
  opts.allow_trailing_comma = true;
  opts.lossless_number = true;
  const auto doc = xsel::query::parse_json(raw, opts);
  CHECK(doc["a"].as<int>() == 1);
  REQUIRE(doc["b"].is_array());
  CHECK(doc["b"].size() == 2);
  CHECK(doc["c"].as<double>() == 123.45);

  const std::string compact = xsel::query::dump_json(doc, false);
  CHECK(compact.find('\n') == std::string::npos);
  const std::string pretty = xsel::query::dump_json(doc, true, 2);
  CHECK(pretty.find('\n') != std::string::npos);
}

TEST_CASE("json_query supports jsonschema validation", "[query]") {
  const auto schema = jsoncons::json::parse(R"({
    "type": "object",
    "required": ["title", "price"],
    "properties": {
      "title": {"type": "string"},
      "price": {"type": "number", "minimum": 0}
    }
  })");

  const auto valid_doc = jsoncons::json::parse(R"({"title": "Lamp", "price": 12.5})");
  std::vector<std::string> errors;
  CHECK(xsel::query::validate_json_schema(valid_doc, schema, &errors));
  CHECK(errors.empty());

  const auto bad_doc = jsoncons::json::parse(R"({"title": "Lamp", "price": -5})");
  CHECK_FALSE(xsel::query::validate_json_schema(bad_doc, schema, &errors));
  CHECK_FALSE(errors.empty());
}

TEST_CASE("json_query supports CSV encode and decode", "[query]") {
  const std::string csv_in = "name,score\nAda,10\nBob,20\n";
  const auto csv_json = xsel::query::decode_csv(csv_in);
  REQUIRE(csv_json.is_array());
  REQUIRE(csv_json.size() == 2);
  CHECK(csv_json[0]["name"].as<std::string>() == "Ada");
  CHECK(csv_json[1]["score"].as<int>() == 20);

  const std::string csv_out = xsel::query::encode_csv(csv_json);
  CHECK(csv_out.find("Ada,10") != std::string::npos);
}

TEST_CASE("json_query handles nesting depth, trailing commas, and comments",
          "[query][edge]") {
  // Unterminated block comment with allow_comments=true should not hang/crash
  xsel::query::JsonParseOptions opts;
  opts.allow_comments = true;
  const auto parsed =
      xsel::query::parse_json(R"({"a": 1} /* unterminated)", opts);
  CHECK(parsed["a"].as<int>() == 1);

  // Nesting depth enforcement
  xsel::query::JsonParseOptions depth_opts;
  depth_opts.max_nesting_depth = 2;
  CHECK_THROWS_AS(xsel::query::parse_json("[[[1]]]", depth_opts),
                  std::runtime_error);

  // Trailing comma & Inf/NaN options
  xsel::query::JsonParseOptions relaxed;
  relaxed.allow_trailing_comma = true;
  const auto tc = xsel::query::parse_json(R"({"a": [1, 2,],})", relaxed);
  CHECK(tc["a"].size() == 2);

  // dump_json indent clamping
  const auto doc = jsoncons::json::parse(R"({"a": 1})");
  CHECK_NOTHROW(xsel::query::dump_json(doc, true, -5));
  CHECK_NOTHROW(xsel::query::dump_json(doc, true, 100));
}
