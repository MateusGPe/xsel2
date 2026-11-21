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

TEST_CASE("Phase 1 DSL: fields, find_first, try_find, attr fallback, tee, save/restore, map_values, query_params, js_blocks aliases, coalesce_keys",
          "[dsl][phase1]") {
  const std::string dsl = R"DSL(
    p1 := Scraper()
      .select("html") {
        fields({
          title: "h1",
          cover: { sel: "img.hero", attr: ["src", "data-src"] },
          missing_opt: { sel: ".missing", default: "N/A" },
          empty_fallback: { sel: ".missing-two", default: "default_val" },
          is_hd: { sel: ".hd", type: exists },
          views: { sel: ".views", cast: "int" },
          tags: { sel: ".tag", first: false, join: ", " }
        })
        | try_find(".maybe-missing", Value.Text) | set("maybe")
        | find_first(".subtitle") | set("subtitle")
        | find_all(".rank-label", Value.Text) | squish
            | map_values({"monthly rank": "ibox_rank_monthly", "weekly rank": "ibox_rank_weekly"})
            | set("mapped_ranks")
        | find_all("script", Value.Text) | strip_comments | squish
            | js_blocks(["VIDEO_SHOW", "flashvars_*"])
        | find_all("script", Value.Text) | filter(["ads_batch"]) | regex_extract("(https://[^\"']+)", 1) | first(1)
            | save("ad_url")
            | tee {
                query_params({clientType: "client_type", "channel[info]": "info_raw"})
              }
            | restore("ad_url")
            | query_param("channel[info]") | json_parse("info")
      }
      .select("a.thumb", "thumbs") {
        @["data-src", "src"] | set("img_url")
      }
      .coalesce_keys({
        final_title: ["VIDEO_SHOW.title", "title"],
        cover_out: ["cover", "missing_opt"]
      });
  )DSL";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("p1"));

  const std::string html = R"(
    <html><body>
      <h1>  Main   Title  </h1>
      <p class="subtitle">  Sub   Title </p>
      <img class="hero" data-src="https://cdn/hero.jpg" />
      <span class="views">42</span>
      <span class="hd"></span>
      <span class="tag">t1</span>
      <span class="tag">t2</span>
      <span class="rank-label">monthly rank</span>
      <span class="rank-label">weekly rank</span>
      <a class="thumb" src="https://cdn/a.jpg"></a>
      <script>
        var VIDEO_SHOW = {"title": "Show Title"};
        var flashvars_999 = {"id": 999};
        var u = "https://ads.test/_xa/ads_batch?clientType=mobile&channel[info]=%7B%22ok%22%3Atrue%7D";
      </script>
    </body></html>
  )";

  const auto out = scrapers["p1"].scrape_from_string(html);
  CHECK(out["final_title"].as<std::string>() == "Show Title");
  CHECK(out["cover_out"].as<std::string>() == "https://cdn/hero.jpg");
  CHECK_FALSE(out.contains("cover"));
  CHECK_FALSE(out.contains("missing_opt"));
  CHECK(out["empty_fallback"].as<std::string>() == "default_val");
  CHECK(out["is_hd"].as<bool>() == true);
  CHECK(out["views"].as<int>() == 42);
  CHECK(out["tags"].as<std::string>() == "t1, t2");
  CHECK(out["subtitle"].as<std::string>() == "Sub Title");
  CHECK_FALSE(out.contains("maybe"));
  REQUIRE(out["mapped_ranks"].is_array());
  CHECK(out["mapped_ranks"][0].as<std::string>() == "ibox_rank_monthly");
  CHECK(out["mapped_ranks"][1].as<std::string>() == "ibox_rank_weekly");
  CHECK(out["flashvars"]["id"].as<int>() == 999);
  CHECK(out["client_type"].as<std::string>() == "mobile");
  CHECK(out["info"]["ok"].as<bool>() == true);
  CHECK(out["thumbs"]["img_url"].as<std::string>() == "https://cdn/a.jpg");
}

TEST_CASE("Phase 2 DSL: pipe/use, include, each, when_exists/unless_exists/when_match, guards, finalize",
          "[dsl][phase2]") {
  const std::string dsl = R"DSL(
    pipe link_map = {
      text | squish -> "@key"
      | @href | squish -> "@value"
    };

    base_meta := Scraper()
      .select("html") {
        find_first("h1") -> "page_title"
        | abort_if_exists(".removed", "status", "removed")
      };

    catalog := Scraper()
      .include(base_meta)
      .select(".card", "items") {
        skip_if_exists(".ad")
        | find_first(".name") | skip_if_empty | set("name")
        | when_exists(".price") {
            find_first(".price") | when_match("\\$") { replace_exact("$", "") } -> "price"
          }
        | unless_exists(".price") {
            default("free") -> "price"
          }
        | each(".links a", "links") { use(link_map) }
        | each(".variants .v", "variants") {
            find_first(".sku") -> "sku"
            | each(".opts span", "opts") { text | squish -> "label" }
          }
      }
      .select(".nonexistent-section") {
        text -> "never"
      }
      .finalize {
        coalesce_keys({heading: ["missing_heading", "page_title"]})
      };
  )DSL";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("catalog"));

  const std::string html = R"(
    <html><body>
      <h1>  Store Catalog  </h1>
      <div class="card ad"><span class="name">Buy Now Ad</span></div>
      <div class="card">
        <span class="name">Keyboard</span>
        <span class="price">$99</span>
        <div class="links"><a href="/docs">Docs</a><a href="/buy">Buy</a></div>
        <div class="variants">
          <div class="v"><span class="sku">K1</span><div class="opts"><span>Red</span><span>Blue</span></div></div>
        </div>
      </div>
      <div class="card">
        <span class="name">Manual</span>
      </div>
    </body></html>
  )";

  const auto out = scrapers["catalog"].scrape_from_string(html);
  // finalize ran even though final .nonexistent-section matched 0 nodes!
  CHECK(out["heading"].as<std::string>() == "Store Catalog");
  CHECK_FALSE(out.contains("page_title"));
  REQUIRE(out["items"].is_array());
  REQUIRE(out["items"].size() == 2);
  CHECK(out["items"][0]["name"].as<std::string>() == "Keyboard");
  CHECK(out["items"][0]["price"].as<std::string>() == "99");
  CHECK(out["items"][0]["links"]["Docs"].as<std::string>() == "/docs");
  CHECK(out["items"][0]["links"]["Buy"].as<std::string>() == "/buy");
  CHECK(out["items"][0]["variants"]["sku"].as<std::string>() == "K1");
  REQUIRE(out["items"][0]["variants"]["opts"].is_array());
  CHECK(out["items"][0]["variants"]["opts"].size() == 2);
  CHECK(out["items"][1]["name"].as<std::string>() == "Manual");
  CHECK(out["items"][1]["price"].as<std::string>() == "free");

  // abort_if_exists stops extraction early while preserving status & finalize
  const std::string removed_html = R"(
    <html><body>
      <h1>Gone</h1>
      <div class="removed"></div>
      <div class="card"><span class="name">Hidden</span></div>
    </body></html>
  )";
  const auto removed_out = scrapers["catalog"].scrape_from_string(removed_html);
  CHECK(removed_out["status"].as<std::string>() == "removed");
  CHECK_FALSE(removed_out.contains("items"));
  CHECK(removed_out["heading"].as<std::string>() == "Gone");
}

TEST_CASE("Phase 3 DSL: regex_find_all, regex_groups, compact numbers, parse_duration, relative dates, media/url helpers, index_json_ld",
          "[dsl][phase3]") {
  const std::string dsl = R"DSL(
    wild := Scraper()
      .select("html") {
        extract_meta(key="ld", include_json_ld=true, index_json_ld=true)
        | find_first(".views") | parse_number(compact=true, key="views")
        | find_first(".dur") | parse_duration(key="duration_sec")
        | find_first(".ago") | parse_date(output="date", now="2026-11-20T12:00:00Z") -> "posted_date"
        | find_first(".hero", Value.Attribute, "srcset") | parse_srcset("largest") -> "hero_img"
        | find_first(".bg", Value.Attribute, "style") | css_url -> "bg_img"
        | find_first(".sku") | regex_groups("(?P<cat>[a-z]+)-(?P<num>\\d+)", prefix="sku_", clear=true)
        | find_first(".nums") | regex_find_all("\\d+") -> "all_nums"
        | find_first(".enc") | unescape_html -> "decoded"
        | find_all("a", Value.Attribute, "href")
            | filter_urls(allowed_domains=["example.com"], blocked_domains=["track.example.com"])
            | strip_query(drop_params=["utm_*", "fbclid"]) -> "clean_links"
      };
  )DSL";

  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse(dsl);
  REQUIRE(scrapers.contains("wild"));

  const std::string html = R"HTML(
    <html>
      <head>
        <script type="application/ld+json">{"@type": "Product", "name": "Widget"}</script>
      </head>
      <body>
        <span class="views">2.5M</span>
        <span class="dur">PT5M30S</span>
        <span class="ago">2 days ago</span>
        <img class="hero" srcset="/a.jpg 320w, /b.jpg 1080w" />
        <div class="bg" style="background: url('/bg.webp')"></div>
        <span class="sku">kbd-900</span>
        <span class="nums">x1 y22 z333</span>
        <span class="enc">A &amp; B</span>
        <a href="https://shop.example.com/item?id=5&utm_source=ad&fbclid=99">Keep</a>
        <a href="https://track.example.com/pixel?id=1">Blocked</a>
        <a href="https://other.net/out">External</a>
      </body>
    </html>
  )HTML";

  const auto out = scrapers["wild"].scrape_from_string(html);
  CHECK(out["ld"]["json_ld_by_type"]["Product"]["name"].as<std::string>() == "Widget");
  CHECK(out["views"].as<int64_t>() == 2500000);
  CHECK(out["duration_sec"].as<int64_t>() == 330);
  CHECK(out["posted_date"].as<std::string>() == "2026-11-18");
  CHECK(out["hero_img"].as<std::string>() == "/b.jpg");
  CHECK(out["bg_img"].as<std::string>() == "/bg.webp");
  CHECK(out["sku_cat"].as<std::string>() == "kbd");
  CHECK(out["sku_num"].as<std::string>() == "900");
  REQUIRE(out["all_nums"].is_array());
  CHECK(out["all_nums"].size() == 3);
  CHECK(out["decoded"].as<std::string>() == "A & B");
  CHECK(out["clean_links"].as<std::string>() == "https://shop.example.com/item?id=5");
}
