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
