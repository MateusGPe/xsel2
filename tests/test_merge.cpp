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
