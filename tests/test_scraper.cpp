#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

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
  builder.Select("h1", "")
      .Text()
      .Squish()
      .Set("title")
      .Select("div.item", "items")
      .Find(".name", scraper::QueryAllStep::Type::Text)
      .Set("name")
      .FindAll(".tag", scraper::QueryAllStep::Type::Text)
      .Count("tag_count")
      .Set("tags")
      .Select("script[type='application/json']", "config")
      .Text()
      .ParseJs("", true);

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
      .select("div.item", "items")
      .find(".name", Value.Text).set("name")
      .find(".price", Value.Text).set("price")
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

TEST_CASE("Scraper supports ErrorPolicy modes and multiple postponed steps",
          "[scraper][error_policy]") {
  const std::string html = R"(<div><span class="x">hello</span></div>)";

  // 1. CollectErrors captures step errors without throwing or writing to cerr
  scraper::ScraperBuilder collect_sb;
  collect_sb.set_error_policy(scraper::ErrorPolicy::CollectErrors);
  collect_sb.Select("div")
      .Find(".missing", scraper::QueryAllStep::Type::Text)
      .Set("val");
  const auto collected_out = collect_sb.scrape_from_string(html);
  CHECK(collected_out.empty());
  CHECK(collect_sb.errors().size() == 1);

  // 2. ThrowOnError propagates step exceptions
  scraper::ScraperBuilder throw_sb;
  throw_sb.set_error_policy(scraper::ErrorPolicy::ThrowOnError);
  throw_sb.Select("div")
      .Find(".missing", scraper::QueryAllStep::Type::Text)
      .Set("val");
  CHECK_THROWS_AS((void)throw_sb.scrape_from_string(html), std::runtime_error);

  // 3. Multiple postponed steps in a single extractor run in order at the end
  scraper::ScraperBuilder multi_post;
  multi_post.Select("div")
      .Find(".x", scraper::QueryAllStep::Type::Text)
      .Set("val")
      .OmitKeys({"other"})
      .RenameKeys({{"val", "renamed"}});
  const auto post_out = multi_post.scrape_from_string(html);
  REQUIRE(post_out.contains("renamed"));
  CHECK(post_out["renamed"].as<std::string>() == "hello");
}

TEST_CASE("Scraper handles LogAndContinue, postponed step errors, empty and malformed HTML",
          "[scraper][edge]") {
  const std::string html = R"(<div><span class="x">hello</span></div>)";

  // LogAndContinue writes formatted error to custom stream
  scraper::ScraperBuilder log_sb;
  log_sb.set_error_policy(scraper::ErrorPolicy::LogAndContinue);
  log_sb.Select("div")
      .Find(".missing", scraper::QueryAllStep::Type::Text)
      .Set("val");
  Document doc;
  doc.parseMemory(html);
  jsoncons::json out = jsoncons::json::object();
  std::ostringstream err_stream;
  log_sb.scrape_document(doc, out, "test_ctx", err_stream);
  CHECK(err_stream.str().find("[test_ctx]") != std::string::npos);
  CHECK(err_stream.str().find("Extractor error") != std::string::npos);

  // Postponed step failure under CollectErrors vs ThrowOnError
  scraper::ScraperBuilder post_err;
  post_err.set_error_policy(scraper::ErrorPolicy::CollectErrors);
  post_err.Select("div")
      .Find(".x", scraper::QueryAllStep::Type::Text)
      .Set("val")
      .JmesPath("invalid[[[");
  (void)post_err.scrape_from_string(html);
  CHECK(post_err.errors().size() == 1);

  post_err.set_error_policy(scraper::ErrorPolicy::ThrowOnError);
  CHECK_THROWS_AS((void)post_err.scrape_from_string(html), std::runtime_error);

  // Empty HTML and malformed unclosed tags
  scraper::ScraperBuilder basic;
  basic.Select("p").Text().Squish().Set("p");
  CHECK(basic.scrape_from_string("").empty());
  const auto malformed = basic.scrape_from_string("<div><p>unclosed <b>bold");
  REQUIRE(malformed.contains("p"));
  CHECK(malformed["p"].as<std::string>() == "unclosed bold");
}

TEST_CASE("Hardening: copied ScraperBuilder does not share mutable Extractor state",
          "[scraper][hardening]") {
  scraper::ScraperBuilder s1;
  s1.Select("h1").Text();

  scraper::ScraperBuilder s2 = s1;
  s1.Lower().Set("title");
  s2.Upper().Set("title");

  const std::string html = "<h1>Hello</h1>";
  const auto r1 = s1.scrape_from_string(html);
  const auto r2 = s2.scrape_from_string(html);
  CHECK(r1["title"].as<std::string>() == "hello");
  CHECK(r2["title"].as<std::string>() == "HELLO");
}
