#include "scraper/merge.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("split_dict_path handles escaped dots", "[merge]") {
  const auto segs = scraper::split_dict_path(R"(user.profile\.meta.name)", true);
  REQUIRE(segs.size() == 3);
  CHECK(segs[0] == "user");
  CHECK(segs[1] == "profile.meta");
  CHECK(segs[2] == "name");
}

TEST_CASE("insert_into_json supports InsertMode variants", "[merge]") {
  jsoncons::json dest = jsoncons::json::object();

  scraper::insert_into_json(dest, "tags", jsoncons::json("news"),
                            {scraper::InsertMode::AppendToList});
  scraper::insert_into_json(dest, "tags", jsoncons::json("tech"),
                            {scraper::InsertMode::AppendToList});
  REQUIRE(dest["tags"].is_array());
  CHECK(dest["tags"].size() == 2);

  scraper::insert_into_json(dest, "meta.count", jsoncons::json(10),
                            {scraper::InsertMode::Overwrite});
  scraper::insert_into_json(dest, "meta.count", jsoncons::json(20),
                            {scraper::InsertMode::Overwrite});
  CHECK(dest["meta"]["count"].as<int64_t>() == 20);

  scraper::insert_into_json(dest, "item", jsoncons::json("first"),
                            {scraper::InsertMode::UniqueSuffix});
  scraper::insert_into_json(dest, "item", jsoncons::json("second"),
                            {scraper::InsertMode::UniqueSuffix});
  CHECK(dest["item"].as<std::string>() == "first");
  CHECK(dest["item_0"].as<std::string>() == "second");
}

TEST_CASE("merge_into transforms @key/@value objects", "[merge]") {
  jsoncons::json dest = jsoncons::json::object();
  jsoncons::json item = jsoncons::json::parse(R"({"@key": "price", "@value": 19.99})");
  scraper::merge_into(dest, std::move(item));
  REQUIRE(dest.contains("price"));
  CHECK(dest["price"].as<double>() == 19.99);
}

TEST_CASE("has_no_common_key and merge_into handle Merge mode and arrays",
          "[merge]") {
  const auto a = jsoncons::json::parse(R"({"x": 1, "y": 2})");
  const auto b = jsoncons::json::parse(R"({"z": 3})");
  const auto c = jsoncons::json::parse(R"({"y": 9})");
  CHECK(scraper::has_no_common_key(a, b));
  CHECK_FALSE(scraper::has_no_common_key(a, c));

  // InsertMode::Merge recursively merges nested objects
  jsoncons::json dest = jsoncons::json::parse(R"({"meta": {"a": 1}, "tags": ["x"]})");
  jsoncons::json other = jsoncons::json::parse(R"({"meta": {"b": 2}, "tags": ["y"]})");
  scraper::merge_into(dest, std::move(other), scraper::InsertMode::Merge);
  CHECK(dest["meta"]["a"].as<int64_t>() == 1);
  CHECK(dest["meta"]["b"].as<int64_t>() == 2);
  REQUIRE(dest["tags"].is_array());
  CHECK(dest["tags"].size() == 2);

  // Array + scalar merge
  jsoncons::json arr = jsoncons::json::parse("[1, 2]");
  scraper::merge_into(arr, jsoncons::json(3));
  REQUIRE(arr.is_array());
  CHECK(arr.size() == 3);

  // Incomplete @key without @value is stripped
  jsoncons::json orphan = jsoncons::json::parse(R"({"@key": "only_key", "keep": 1})");
  scraper::preprocess_key_value(orphan);
  CHECK_FALSE(orphan.contains("@key"));
  CHECK(orphan["keep"].as<int64_t>() == 1);
}

TEST_CASE("merge_item_at_path handles nested object and array destinations",
          "[merge]") {
  jsoncons::json obj = jsoncons::json::null();
  scraper::merge_item_at_path(obj, {"a", "b"}, jsoncons::json(42));
  REQUIRE(obj.is_object());
  CHECK(obj["a"]["b"].as<int>() == 42);

  jsoncons::json arr = jsoncons::json::array();
  scraper::merge_item_at_path(arr, {"item", "name"}, jsoncons::json("Widget"));
  REQUIRE(arr.is_array());
  REQUIRE(arr.size() == 1);
  CHECK(arr[0]["item"]["name"].as<std::string>() == "Widget");
}

TEST_CASE("merge handles path edge cases, allow_empty, @key/@value variants, and depth limits",
          "[merge][edge]") {
  // nested_path=false keeps dots literal; trailing backslash preserved
  const auto lit = scraper::split_dict_path("a.b.c", false);
  REQUIRE(lit.size() == 1);
  CHECK(lit[0] == "a.b.c");

  const auto bs = scraper::split_dict_path(R"(a\)", true);
  REQUIRE(bs.size() == 1);
  CHECK(bs[0] == R"(a\)");

  // allow_empty=false skips empty values; allow_empty=true inserts them
  jsoncons::json dest = jsoncons::json::object();
  scraper::insert_into_json(dest, "empty_skip", jsoncons::json(""),
                            {scraper::InsertMode::Overwrite, false, true});
  CHECK_FALSE(dest.contains("empty_skip"));
  scraper::insert_into_json(dest, "empty_keep", jsoncons::json(""),
                            {scraper::InsertMode::Overwrite, true, true});
  REQUIRE(dest.contains("empty_keep"));
  CHECK(dest["empty_keep"].as<std::string>().empty());

  // Intermediate non-object segment is replaced by object
  dest["a"] = 123;
  scraper::insert_into_json(dest, "a.b", jsoncons::json("ok"),
                            {scraper::InsertMode::Overwrite});
  REQUIRE(dest["a"].is_object());
  CHECK(dest["a"]["b"].as<std::string>() == "ok");

  // @value without @key and empty @key are stripped
  jsoncons::json kv_edge = jsoncons::json::parse(
      R"({"@value": 10, "nested": {"@key": "", "@value": 5, "ok": 1}})");
  scraper::preprocess_key_value(kv_edge);
  CHECK_FALSE(kv_edge.contains("@value"));
  CHECK_FALSE(kv_edge["nested"].contains("@key"));
  CHECK(kv_edge["nested"]["ok"].as<int>() == 1);

  // Scalar + scalar merge_into produces array
  jsoncons::json s = jsoncons::json("first");
  scraper::merge_into(s, jsoncons::json("second"));
  REQUIRE(s.is_array());
  CHECK(s.size() == 2);

  // Depth limit guard
  jsoncons::json deep = 1;
  for (int i = 0; i < xsel::common::MAX_JSON_DEPTH + 5; ++i) {
    jsoncons::json wrap = jsoncons::json::array();
    wrap.push_back(std::move(deep));
    deep = std::move(wrap);
  }
  jsoncons::json target = jsoncons::json::object();
  CHECK_THROWS_AS(scraper::merge_into(target, std::move(deep)),
                  std::runtime_error);
}
