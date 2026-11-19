#include "scraper/dsl_parser.hpp"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

TEST_CASE("DslParser supports variables, :=, grouped chains, and dict steps",
          "[dsl]") {
  const std::string dsl = R"(
    # Hash comment
    // Slash comment
    let sel = "div.card";

    cards := Scraper()
      .push(sel, "cards")
      .(
        .query(".title", Value.Text).trim().set("title")
        .query(".tag", Value.Text).set("tags")
      )
      .rename_keys(["title", "heading"], false)
      .merge_keys(["heading", "tags"], "summary", false)
      .sort_keys(true, true);
  )";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("cards"));

  const std::string html = R"(
    <div class="card">
      <span class="title">  Alpha  </span>
      <span class="tag">t1</span>
    </div>
    <div class="card">
      <span class="title">  Beta  </span>
      <span class="tag">t2</span>
    </div>
  )";

  const auto res = scrapers["cards"].scrape_from_string(html);
  REQUIRE(res["cards"].is_array());
  REQUIRE(res["cards"].size() == 2);
  REQUIRE(res["cards"][0]["summary"].is_array());
  CHECK(res["cards"][0]["summary"][0].as<std::string>() == "Alpha");
  CHECK(res["cards"][0]["summary"][1].as<std::string>() == "t1");
  CHECK(res["cards"][1]["summary"][0].as<std::string>() == "Beta");
}

TEST_CASE("DslParser reports syntax, method, and file errors", "[dsl]") {
  scraper::dsl::DslParser parser;

  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().unknown_method();)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().push("unterminated);)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().rename_keys(["only_one"]);)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse_file("/nonexistent/path/to/scrapers.dsl"),
                  std::runtime_error);
}
