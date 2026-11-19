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

TEST_CASE("json_query supports jsonpath_replace, flatten, and unflatten",
          "[query]") {
  const auto doc = jsoncons::json::parse(
      R"({"items": [{"name": "a", "price": 10}, {"name": "b", "price": 20}]})");
  const auto replaced =
      xsel::query::jsonpath_replace(doc, "$.items[?(@.price > 15)].price", 99);
  CHECK(replaced["items"][0]["price"].as<int>() == 10);
  CHECK(replaced["items"][1]["price"].as<int>() == 99);

  const auto flat = xsel::query::json_flatten(doc);
  CHECK(flat.is_object());
  CHECK(flat["$['items'][0]['name']"].as<std::string>() == "a");

  const auto unflat = xsel::query::json_unflatten(flat);
  CHECK(unflat == doc);
}

TEST_CASE("json_query supports jsonpointer, jsonpatch, and mergepatch",
          "[query]") {
  const auto doc = jsoncons::json::parse(R"({"user": {"name": "Ada", "age": 30}})");

  CHECK(xsel::query::jsonpointer_get(doc, "/user/name").as<std::string>() ==
        "Ada");
  CHECK_THROWS_AS(xsel::query::jsonpointer_get(doc, "/user/missing"),
                  std::runtime_error);

  const auto set_doc =
      xsel::query::jsonpointer_set(doc, "/user/role", jsoncons::json("admin"));
  CHECK(set_doc["user"]["role"].as<std::string>() == "admin");

  const auto rem_doc = xsel::query::jsonpointer_remove(set_doc, "/user/age");
  CHECK_FALSE(rem_doc["user"].contains("age"));

  const auto patch = jsoncons::json::parse(
      R"([{"op": "replace", "path": "/user/name", "value": "Grace"}])");
  const auto patched = xsel::query::apply_json_patch(doc, patch);
  CHECK(patched["user"]["name"].as<std::string>() == "Grace");

  const auto mpatch = jsoncons::json::parse(R"({"user": {"age": null, "city": "NYC"}})");
  const auto mpatched = xsel::query::apply_merge_patch(doc, mpatch);
  CHECK_FALSE(mpatched["user"].contains("age"));
  CHECK(mpatched["user"]["city"].as<std::string>() == "NYC");
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

TEST_CASE("json_query supports CSV and binary formats", "[query]") {
  const std::string csv_in = "name,score\nAda,10\nBob,20\n";
  const auto csv_json = xsel::query::decode_csv(csv_in);
  REQUIRE(csv_json.is_array());
  REQUIRE(csv_json.size() == 2);
  CHECK(csv_json[0]["name"].as<std::string>() == "Ada");
  CHECK(csv_json[1]["score"].as<int>() == 20);

  const std::string csv_out = xsel::query::encode_csv(csv_json);
  CHECK(csv_out.find("Ada,10") != std::string::npos);

  const auto sample = jsoncons::json::parse(R"({"k": "hello", "n": 42})");
  for (auto fmt : {xsel::query::BinaryFormat::Cbor,
                   xsel::query::BinaryFormat::MsgPack,
                   xsel::query::BinaryFormat::Bson,
                   xsel::query::BinaryFormat::Ubjson}) {
    for (auto enc : {xsel::query::BinaryEncoding::Hex,
                     xsel::query::BinaryEncoding::Base64,
                     xsel::query::BinaryEncoding::Raw}) {
      const std::string encoded = xsel::query::encode_binary(fmt, sample, enc);
      const auto decoded = xsel::query::decode_binary(fmt, encoded, enc);
      CHECK(decoded == sample);
    }
  }
}

TEST_CASE("json_query rejects malformed hex, base64, and failing patches",
          "[query][edge]") {
  CHECK_THROWS_AS(xsel::query::decode_binary(xsel::query::BinaryFormat::Cbor,
                                             "abc",
                                             xsel::query::BinaryEncoding::Hex),
                  std::runtime_error);
  CHECK_THROWS_AS(xsel::query::decode_binary(xsel::query::BinaryFormat::Cbor,
                                             "zz",
                                             xsel::query::BinaryEncoding::Hex),
                  std::runtime_error);
  CHECK_THROWS_AS(
      xsel::query::decode_binary(xsel::query::BinaryFormat::Cbor, "abc",
                                 xsel::query::BinaryEncoding::Base64),
      std::runtime_error);

  const auto doc = jsoncons::json::parse(R"({"a": 1})");
  CHECK_THROWS_AS(xsel::query::jsonpointer_get(doc, "/missing"),
                  std::runtime_error);
  const auto bad_patch = jsoncons::json::parse(
      R"([{"op": "test", "path": "/a", "value": 2}])");
  CHECK_THROWS_AS(xsel::query::apply_json_patch(doc, bad_patch),
                  std::runtime_error);

  // Mid-stream Base64 padding must be rejected
  CHECK_THROWS_AS(
      xsel::query::decode_binary(xsel::query::BinaryFormat::Cbor, "AA==AAAA",
                                 xsel::query::BinaryEncoding::Base64),
      std::runtime_error);

  // Unterminated block comment with allow_comments=true should not hang/crash
  xsel::query::JsonParseOptions opts;
  opts.allow_comments = true;
  const auto parsed =
      xsel::query::parse_json(R"({"a": 1} /* unterminated)", opts);
  CHECK(parsed["a"].as<int>() == 1);
}
