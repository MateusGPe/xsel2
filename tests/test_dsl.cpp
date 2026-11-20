#include "scraper/dsl_parser.hpp"
#include "scraper/dsl/value.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
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

TEST_CASE("DslParser supports universal idioms: |, $, @, ??, [i], as, paren-free steps, and plain-English aliases",
          "[dsl][sugar]") {
  const std::string dsl = R"dsl(
    product: Scraper()
      | $("h1") { text | clean as "title" }
      | $ "a.docs" { @href | prepend("https://example.com") -> "manual_url" }
      | $(".price") { text | extract("([0-9]+\\.[0-9]+)", 1) ?? "0.00" -> "price" }
      | $("body") {
          find_all(".missing", text) ?? "N/A" as("fallback")
          | find_all(".tag", text) | strip | dedup | limit(2) -> "tags"
          | find_all("li.spec", text) | strip | [0] -> "first_spec"
          | find_all("li.spec", text) | strip | [-1] -> "last_spec"
          | find_all("p.note", text) | grep("keep") | sub("keep:", "") | strip -> "notes"
        }
      | $("script#cfg") { text | json("cfg", allow_comments=true) }
      | $("script#js") { text | json("js_cfg", lenient=true) }
  )dsl";

  const std::string html = R"(
    <html><body>
      <h1>  Hello   World  </h1>
      <a class="docs" href="/guide">Guide</a>
      <span class="price">USD 19.99</span>
      <span class="tag">  c++ </span>
      <span class="tag">c++</span>
      <span class="tag">dsl</span>
      <span class="tag">extra</span>
      <ul>
        <li class="spec">  fast  </li>
        <li class="spec">  light  </li>
        <li class="spec">  typed  </li>
      </ul>
      <p class="note">drop me</p>
      <p class="note">keep:ok</p>
      <script id="cfg">{ /* ok */ "v": 2 }</script>
      <script id="js">{ unquoted: 'yes', items: [1, 2,], }</script>
    </body></html>
  )";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("product"));

  const auto out = scrapers.at("product").scrape_from_string(html);
  CHECK(out["title"].as<std::string>() == "Hello World");
  CHECK(out["manual_url"].as<std::string>() == "https://example.com/guide");
  CHECK(out["price"].as<std::string>() == "19.99");
  CHECK(out["fallback"].as<std::string>() == "N/A");
  REQUIRE(out["tags"].is_array());
  REQUIRE(out["tags"].size() == 2);
  CHECK(out["tags"][0].as<std::string>() == "c++");
  CHECK(out["tags"][1].as<std::string>() == "dsl");
  CHECK(out["first_spec"].as<std::string>() == "fast");
  CHECK(out["last_spec"].as<std::string>() == "typed");
  CHECK(out["notes"].as<std::string>() == "ok");
  CHECK(out["cfg"]["v"].as<int>() == 2);
  CHECK(out["js_cfg"]["unquoted"].as<std::string>() == "yes");
  REQUIRE(out["js_cfg"]["items"].is_array());
  CHECK(out["js_cfg"]["items"].size() == 2);
}

TEST_CASE("DslParser validates new sugar edge cases and errors", "[dsl][sugar]") {
  scraper::dsl::DslParser parser;
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("h1") { text ? "x" };)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("li") { text | [-2] };)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | select;)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("li").nth(-1);)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("li").nth("12abc");)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("li").slice(-1, 2);)"),
                  std::runtime_error);
}

TEST_CASE("DslParser executes Tier 1 steps and aliases", "[dsl][tier1]") {
  const std::string dsl = R"(
    product = Scraper()
      | $(".price")  { text | parse_number(key="price") }
      | $(".qty")    { text | set("qty", type="int") }
      | $("a.buy")   { @href | resolve_url("https://example.com/shop/") | strip_query(["id"]) -> "url" }
      | $(".title")  { text | replace_many({"Pro": "Professional", "Edition": "Ed."}) | slugify() -> "slug" }
      | $(".tags")   { text | extract_many(["USB-C", "4K"], unique=true) | sort() -> "tags" }
      | $(".badge")  { text | coalesce("Standard") -> "badge" }
      | $(".id_src") { text | hash("sha256") -> "id_hash" }
  )";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("product"));

  const std::string html = R"(
    <div class="p">
      <span class="price">$49.50</span>
      <span class="qty">3</span>
      <a class="buy" href="../item?id=77&utm_medium=email">Go</a>
      <h1 class="title">Pro Audio Edition</h1>
      <p class="tags">4K HDR with USB-C</p>
      <span class="badge">   </span>
      <span class="id_src">abc</span>
    </div>
  )";

  const auto res = scrapers["product"].scrape_from_string(html);
  CHECK(std::abs(res["price"].as<double>() - 49.5) < 1e-6);
  CHECK(res["qty"].as<int64_t>() == 3);
  CHECK(res["url"].as<std::string>() == "https://example.com/item?id=77");
  CHECK(res["slug"].as<std::string>() == "professional-audio-ed");
  REQUIRE(res["tags"].is_array());
  CHECK(res["tags"][0].as<std::string>() == "4K");
  CHECK(res["tags"][1].as<std::string>() == "USB-C");
  CHECK(res["badge"].as<std::string>() == "Standard");
  CHECK(res["id_hash"].as<std::string>() ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("DslParser executes Tier 2 steps", "[dsl][tier2]") {
  const std::string dsl = R"DSL(
    page = Scraper()
      | $("head") { extract_meta("meta") }
      | $("#main") { xpath(".//span[@class='sku']/text()") -> "sku" }
      | $("table") { table(key="rows", header=true, normalize_headers=true) }
      | $(".pub") { text | parse_date(output="iso", key="published_iso") }
      | $(".word") { text | unicode_normalize("NFD") | strip_accents() -> "plain" }
      | $(".cand") { find_all("li", "text") | fuzzy_filter("Apple", threshold=0.8) -> "matches" }
  )DSL";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("page"));

  const std::string html = R"(
    <html>
      <head><title>Store</title><meta name="author" content="Ada"></head>
      <body>
        <div id="main"><span class="sku">SKU-42</span></div>
        <table><tr><th>Key Name</th><th>Val</th></tr><tr><td>A</td><td>1</td></tr></table>
        <span class="pub">2026-11-19</span>
        <span class="word">São Paulo</span>
        <ul class="cand"><li>Apple</li><li>Apply</li><li>Banana</li></ul>
      </body>
    </html>
  )";

  const auto res = scrapers["page"].scrape_from_string(html);
  CHECK(res["meta"]["title"].as<std::string>() == "Store");
  CHECK(res["meta"]["author"].as<std::string>() == "Ada");
  CHECK(res["sku"].as<std::string>() == "SKU-42");
  CHECK(res["rows"][0]["key_name"].as<std::string>() == "A");
  CHECK(res["published_iso"].as<std::string>() == "2026-11-19T00:00:00Z");
  CHECK(res["plain"].as<std::string>() == "Sao Paulo");
  REQUIRE(res["matches"].is_array());
  CHECK(res["matches"].size() == 2);
}

TEST_CASE("DslValue guards against non-finite/out-of-range doubles and invalid coercions",
          "[dsl][value][edge]") {
  using scraper::dsl::DslValue;
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(std::numeric_limits<double>::quiet_NaN())),
                  std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(std::numeric_limits<double>::infinity())),
                  std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(1e20)), std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(-1e20)), std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(std::string("12x"))), std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(std::string(""))), std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_int(DslValue(std::string("99999999999999999999"))),
                  std::runtime_error);

  CHECK_THROWS_AS(scraper::dsl::as_bool(DslValue(std::string("maybe"))), std::runtime_error);
  CHECK_THROWS_AS(scraper::dsl::as_non_negative_size(DslValue(-1), "nth"),
                  std::runtime_error);
}

TEST_CASE("DslParser rejects malformed syntax, interpolation errors, and excessive depth",
          "[dsl][edge]") {
  scraper::dsl::DslParser parser;

  // Unterminated string & trailing backslash
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("h1") { text -> "unterminated };)"),
                  std::runtime_error);
  CHECK_THROWS_AS(parser.parse("s = Scraper() | $(\"h1\\"), std::runtime_error);

  // Unknown character
  CHECK_THROWS_AS(parser.parse(R"(s = Scraper() | $("h1") ~ text;)"),
                  std::runtime_error);

  // Duplicate keyword arg & positional after keyword
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | set(key="a", key="b") };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | replace(old="a", "b") };)"),
      std::runtime_error);

  // Interpolation errors & escaped \$
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text -> "${missing}" };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text -> "${unterm" };)"),
      std::runtime_error);
  const auto escaped = parser.parse(
      R"(s = Scraper() | $("h1") { text -> "\${literal}" };)");
  REQUIRE(escaped.contains("s"));

  // Non-finite float literal & unknown step
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | fuzzy_filter("a", threshold=1e999) };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | nonexistent_step() };)"),
      std::runtime_error);

  // Excessive nesting depth
  std::string deep = "s = Scraper() | $(\"h1\") { text | filter(";
  for (int i = 0; i < 80; ++i) deep += "[";
  deep += "\"a\"";
  for (int i = 0; i < 80; ++i) deep += "]";
  deep += ") };";
  CHECK_THROWS_AS(parser.parse(deep), std::runtime_error);
}

TEST_CASE("Hardening: DSL scientific notation and strict argument validation",
          "[dsl][hardening]") {
  scraper::dsl::DslParser parser;

  // Scientific notation floats work and overflowing exponents throw
  auto sci = parser.parse(
      R"(s = Scraper() | $("li") { find_all("span", text) | fuzzy_filter("Apple", threshold=1.5e-1) -> "m" };)");
  REQUIRE(sci.contains("s"));
  const auto sci_res = sci.at("s").scrape_from_string("<li><span>Apple</span></li>");
  CHECK(sci_res["m"].as<std::string>() == "Apple");

  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | fuzzy_filter("a", threshold=1e999) };)"),
      std::runtime_error);

  // Reject unknown keyword args, extra positional args, duplicate positional+kwarg, and bad cast_keys types
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | fuzzy_filter("a", bogus=1) };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text | trim("extra") };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | select("h1", selector="h2") { text -> "t" };)"),
      std::runtime_error);
  CHECK_THROWS_AS(
      parser.parse(R"(s = Scraper() | $("h1") { text -> "a" } | cast_keys({a: 123});)"),
      std::runtime_error);
}
