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
      .select(sel, "cards")
      .(
        .find(".title", Value.Text).trim().set("title")
        .find(".tag", Value.Text).set("tags")
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
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().select("unterminated);)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().rename_keys(["only_one"]);)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse_file("/nonexistent/path/to/scrapers.dsl"),
                  std::runtime_error);
}


TEST_CASE("DslParser supports syntax sugar: pipes, arrows, blocks, kwargs, objects, and interpolation",
          "[dsl][sugar]") {
  const std::string dsl = R"(
    let role = "admin"
    let tag_sel = ".tag"

    # Colon declaration + select block + pipe + arrow + interpolation + kwargs + object literal
    user: Scraper()
      |> select("h1") {
           text() |> squish() -> "title"
         }
      |> select("body") {
           find_all("${tag_sel}", Value.Text) |> trim() >> "tags"
         }
      |> select("script#cfg") {
           text()
           |> json_parse("cfg", allow_comments=true, allow_trailing_comma=true)
           |> jsonpointer_set("/cfg/role", "${role}", postponable=false)
           |> jsonpointer_set("/cfg/weight", 1.5, postponable=false)
           |> jsonpointer_set("/cfg/removed", null, postponable=false)
         }
      |> merge_patch({"meta": {source: "dsl", escaped: "\${role}"}})
      |> rename_keys({"title": "heading"})
      |> json_schema({
           type: "object",
           required: ["heading", "cfg", "meta"]
         })
  )";

  const std::string html = R"(
    <html><body>
      <h1>  Hello World  </h1>
      <span class="tag">c++</span>
      <span class="tag">json</span>
      <script id="cfg">{ /* c */ "active": true, }</script>
    </body></html>
  )";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("user"));

  const auto out = scrapers.at("user").scrape_from_string(html);
  CHECK(out["heading"].as<std::string>() == "Hello World");
  REQUIRE(out["tags"].is_array());
  CHECK(out["tags"].size() == 2);
  CHECK(out["cfg"]["active"].as<bool>() == true);
  CHECK(out["cfg"]["role"].as<std::string>() == "admin");
  CHECK(out["cfg"]["weight"].as<double>() == 1.5);
  CHECK(out["cfg"]["removed"].is_null());
  CHECK(out["meta"]["source"].as<std::string>() == "dsl");
  CHECK(out["meta"]["escaped"].as<std::string>() == "${role}");
}

TEST_CASE("DslParser rejects invalid kwargs and undefined interpolation variables",
          "[dsl][sugar]") {
  scraper::dsl::DslParser parser;
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().select("h1", key="a", "b");)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().select("h1", key="a", key="b");)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper().select("${missing_var}");)"),
                  std::runtime_error);
}

TEST_CASE("DslParser supports long aliases for squish and js_block(s)",
          "[dsl][aliases]") {
  const std::string dsl = R"(
    s = Scraper()
      |> select("h1") { text() |> normalize_whitespace() -> "title" }
      |> select("script#one") { text() |> extract_js_block("data = ") |> parse_js("one") }
      |> select("script#two") { text() |> extract_js_blocks(["app.a", "app.b"]) }
  )";
  const std::string html = R"(
    <h1>  Hello   World  </h1>
    <script id="one">var data = {"ok": true};</script>
    <script id="two">app.a = {"x": 1}; app.b = {"y": 2};</script>
  )";
  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  const auto out = scrapers.at("s").scrape_from_string(html);
  CHECK(out["title"].as<std::string>() == "Hello World");
  CHECK(out["one"]["ok"].as<bool>() == true);
  CHECK(out["app"]["a"]["x"].as<int>() == 1);
  CHECK(out["app"]["b"]["y"].as<int>() == 2);
}
