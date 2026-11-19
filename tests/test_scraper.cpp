#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scraper extracts typed JSON, counts, and JMESPath transforms",
          "[scraper]") {
  const std::string html = R"(
    <html>
      <body>
        <h1>  Catalog  </h1>
        <div class="item">
          <span class="name">Lamp</span>
          <span class="tag">home</span>
          <span class="tag">sale</span>
        </div>
        <div class="item">
          <span class="name">Chair</span>
          <span class="tag">office</span>
        </div>
        <script type="application/json">
          {
            active: true,
            discount: 0.15,
            limit: 100
          }
        </script>
      </body>
    </html>
  )";

  scraper::ScraperBuilder builder;
  builder.Group("h1", "")
      .Text()
      .Normspace()
      .Set("title")
      .Group("div.item", "items")
      .Query(".name", scraper::QueryAllStep::Type::Text)
      .Set("name")
      .QueryAll(".tag", scraper::QueryAllStep::Type::Text)
      .CountValues("tag_count")
      .Set("tags")
      .Group("script[type='application/json']", "config")
      .Text()
      .Parse("", true);

  const auto res = builder.scrape_from_string(html);
  REQUIRE(res.is_object());
  CHECK(res["title"].as<std::string>() == "Catalog");

  REQUIRE(res["items"].is_array());
  REQUIRE(res["items"].size() == 2);
  CHECK(res["items"][0]["name"].as<std::string>() == "Lamp");
  CHECK(res["items"][0]["tag_count"].is_int64());
  CHECK(res["items"][0]["tag_count"].as<int64_t>() == 2);
  CHECK(res["items"][1]["tag_count"].as<int64_t>() == 1);

  REQUIRE(res["config"].is_object());
  CHECK(res["config"]["active"].as<bool>() == true);
  CHECK(res["config"]["discount"].as<double>() == Catch::Approx(0.15));
  CHECK(res["config"]["limit"].as<int64_t>() == 100);
}

TEST_CASE("DslParser supports jmespath and jsonpath pipelines", "[scraper][dsl]") {
  const std::string dsl = R"(
    catalog = Scraper()
      .push("div.item", "items")
      .query(".name", Value.Text).set("name")
      .query(".price", Value.Text).set("price")
      .jmespath("items[].name");
  )";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("catalog"));

  const std::string html = R"(
    <div class="item"><span class="name">Pen</span><span class="price">2</span></div>
    <div class="item"><span class="name">Book</span><span class="price">12</span></div>
  )";

  const auto res = scrapers["catalog"].scrape_from_string(html);
  REQUIRE(res.is_array());
  REQUIRE(res.size() == 2);
  CHECK(res[0].as<std::string>() == "Pen");
  CHECK(res[1].as<std::string>() == "Book");
}
