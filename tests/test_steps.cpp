#include "common/json_utils.hpp"
#include "digger/digger.hpp"
#include "scraper/scraper.hpp"
#include "scraper/steps/block_steps.hpp"
#include "scraper/steps/dict_steps.hpp"
#include "scraper/steps/dom_steps.hpp"
#include "scraper/steps/json_steps.hpp"
#include "scraper/steps/regex_steps.hpp"
#include "scraper/steps/text_steps.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <lxml2-query.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

Node dummy_node() { return Node(nullptr); }

} // namespace

TEST_CASE("text_steps cover trim, strip, case, replace, split, join, affixes",
          "[steps][text]") {
  std::vector<std::string> vals = {"  hello  ", "\t\nworld\r", "   "};
  jsoncons::json dict = jsoncons::json::object();

  scraper::TrimStep().apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"hello", "world", ""});

  scraper::RemoveEmptyStep(true).apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"hello", "world"});

  scraper::UpperStep().apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"HELLO", "WORLD"});

  scraper::LowerStep().apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"hello", "world"});

  scraper::PrependStep("pre_").apply(dummy_node(), vals, dict);
  scraper::AppendStep("_post").apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"pre_hello_post", "pre_world_post"});

  scraper::JoinStep(", ").apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"pre_hello_post, pre_world_post"});

  scraper::ReplaceStep("_", "-").apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"pre-hello_post, pre_world_post"});

  scraper::SubstituteStep("_", "-").apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"pre-hello-post, pre-world-post"});

  scraper::SplitStep(", ").apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"pre-hello-post", "pre-world-post"});

  CHECK_THROWS_AS(scraper::SplitStep(""), std::runtime_error);
}

TEST_CASE("StripStep, SubstringStep, UnescapeStep, and StripCommentsStep",
          "[steps][text]") {
  jsoncons::json dict = jsoncons::json::object();

  std::vector<std::string> strip_vals = {"  foo   bar \n baz  "};
  scraper::StripStep().apply(dummy_node(), strip_vals, dict);
  CHECK(strip_vals[0] == "foo bar baz");

  // SubstringStep: positive, negative, zero length, out-of-bounds
  std::vector<std::string> sub1 = {"abcdef"};
  scraper::SubstringStep(1, 3).apply(dummy_node(), sub1, dict);
  CHECK(sub1[0] == "bcd");

  std::vector<std::string> sub2 = {"abcdef"};
  scraper::SubstringStep(-3).apply(dummy_node(), sub2, dict);
  CHECK(sub2[0] == "def");

  std::vector<std::string> sub3 = {"abcdef"};
  scraper::SubstringStep(1, -1).apply(dummy_node(), sub3, dict);
  CHECK(sub3[0] == "bcde");

  std::vector<std::string> sub_zero = {"abcdef"};
  scraper::SubstringStep(2, 0).apply(dummy_node(), sub_zero, dict);
  CHECK(sub_zero[0].empty());

  std::vector<std::string> sub_oob = {"abc"};
  scraper::SubstringStep(10, 5).apply(dummy_node(), sub_oob, dict);
  CHECK(sub_oob[0].empty());

  // UnescapeStep with surrogate pair + URL decode
  std::vector<std::string> esc = {R"(hi\n\u0041\uD83D\uDE00%20ok)"};
  scraper::UnescapeStep(true).apply(dummy_node(), esc, dict);
  CHECK(esc[0] == "hi\nA\xF0\x9F\x98\x80 ok");

  // StripCommentsStep preserves comments inside quotes
  std::vector<std::string> code = {
      R"(const a = "//keep"; // drop
/* multi */ const b = '/*keep*/';)"};
  scraper::StripCommentsStep(true, true).apply(dummy_node(), code, dict);
  CHECK(code[0] == "const a = \"//keep\"; \n const b = '/*keep*/';");
}

TEST_CASE("regex_steps cover replace, filter, extract, and AhoCorasick filter",
          "[steps][regex]") {
  jsoncons::json dict = jsoncons::json::object();

  std::vector<std::string> vals = {"Item-12", "ITEM-34", "Other"};
  scraper::RegexFilterStep("^item-(\\d+)$", false, std::nullopt)
      .apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"Item-12", "ITEM-34"});

  scraper::RegexReplaceStep("item-", "ID:", std::nullopt)
      .apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"ID:12", "ID:34"});

  scraper::RegexExtractStep("ID:(\\d+)", 1).apply(dummy_node(), vals, dict);
  REQUIRE(vals == std::vector<std::string>{"12", "34"});

  // Group 0 / negative group
  std::vector<std::string> multi = {"k=v1", "nomatch"};
  scraper::RegexExtractStep("(\\w+)=(\\w+)", -1).apply(dummy_node(), multi, dict);
  REQUIRE(multi == std::vector<std::string>{"k", "v1"});

  // FilterStep with +include / -exclude
  std::vector<std::string> fvals = {"apple pie", "apple tart", "banana pie"};
  scraper::FilterStep({"+apple", "-tart"}).apply(dummy_node(), fvals, dict);
  REQUIRE(fvals == std::vector<std::string>{"apple pie"});

  CHECK_THROWS_AS(scraper::RegexReplaceStep("[invalid", "x"),
                  std::invalid_argument);
  CHECK_THROWS_AS(scraper::FilterStep({}), std::runtime_error);
}

TEST_CASE("dom_steps cover Query, QueryAll, Slice, Unique, Default, Set, Count",
          "[steps][dom]") {
  const std::string html = R"(
    <html><body>
      <div id="box" data-kind="main">Hello <span>World</span></div>
      <p class="p">A</p>
      <p class="p">B</p>
      <p class="p">A</p>
      <p class="p">C</p>
    </body></html>
  )";
  Document doc;
  doc.parseMemory(html);
  QueryResult root(doc.getRoot());
  REQUIRE(root[0].has_value());
  Node root_node = *root[0];

  jsoncons::json dict = jsoncons::json::object();
  std::vector<std::string> vals;

  // OwnText vs Text vs Attribute vs Exists
  scraper::QueryStep("#box", scraper::QueryAllStep::Type::OwnText)
      .apply(root_node, vals, dict);
  REQUIRE(vals.size() == 1);
  scraper::TrimStep().apply(root_node, vals, dict);
  CHECK(vals[0] == "Hello");

  vals.clear();
  scraper::QueryStep("#box", scraper::QueryAllStep::Type::Attribute,
                     "data-kind")
      .apply(root_node, vals, dict);
  CHECK(vals == std::vector<std::string>{"main"});

  vals.clear();
  scraper::QueryStep("#missing", scraper::QueryAllStep::Type::ElementExists,
                     "", scraper::QueryStep::Number::Nth, 0)
      .apply(root_node, vals, dict);
  CHECK(vals == std::vector<std::string>{"false"});

  vals.clear();
  scraper::QueryStep("#box", scraper::QueryAllStep::Type::AttributeExists,
                     "data-kind")
      .apply(root_node, vals, dict);
  CHECK(vals == std::vector<std::string>{"true"});

  // Non-existence query with no match throws
  CHECK_THROWS_AS(
      scraper::QueryAllStep("#missing", scraper::QueryAllStep::Type::Text)
          .apply(root_node, vals, dict),
      std::runtime_error);

  // QueryAll + Unique + SliceSelection
  vals.clear();
  scraper::QueryAllStep("p.p", scraper::QueryAllStep::Type::Text)
      .apply(root_node, vals, dict);
  REQUIRE(vals == std::vector<std::string>{"A", "B", "A", "C"});

  scraper::UniqueStep(true).apply(root_node, vals, dict);
  REQUIRE(vals == std::vector<std::string>{"A", "B", "C"});

  auto v_copy = vals;
  scraper::SliceSelectionStep(scraper::SliceSelectionStep::Mode::Last, 2)
      .apply(root_node, v_copy, dict);
  CHECK(v_copy == std::vector<std::string>{"B", "C"});

  v_copy = vals;
  scraper::SliceSelectionStep(scraper::SliceSelectionStep::Mode::Range, 1, 2)
      .apply(root_node, v_copy, dict);
  CHECK(v_copy == std::vector<std::string>{"B", "C"});

  CHECK_THROWS_AS(
      scraper::SliceSelectionStep(scraper::SliceSelectionStep::Mode::Range, 3,
                                  1),
      std::runtime_error);

  // DefaultIfEmpty + Set + CountValues
  vals.clear();
  scraper::DefaultIfEmptyStep("fallback").apply(root_node, vals, dict);
  CHECK(vals == std::vector<std::string>{"fallback"});
  scraper::CountValuesStep("n").apply(root_node, vals, dict);
  scraper::SetStep("item").apply(root_node, vals, dict);
  CHECK(dict["n"].as<int64_t>() == 1);
  CHECK(dict["item"].as<std::string>() == "fallback");
  CHECK(vals.empty());
}

TEST_CASE("block_steps extract and parse delimited JS/JSON blocks",
          "[steps][block]") {
  jsoncons::json dict = jsoncons::json::object();
  std::vector<std::string> vals = {
      R"(window.__STATE__ = { user: { name: "Ana", braces: "{ok}" }, count: 3 };
         window.__META__ = [10, 20];)"};

  auto seeker_vals = vals;
  scraper::BlockSeekerStep("window.__STATE__").apply(dummy_node(), seeker_vals, dict);
  REQUIRE(seeker_vals.size() == 1);
  CHECK(seeker_vals[0].front() == '{');
  CHECK(seeker_vals[0].back() == '}');

  scraper::BlockParserStep({"window.__STATE__", "window.__META__"})
      .apply(dummy_node(), vals, dict);
  CHECK(vals.empty());
  REQUIRE(dict.contains("window"));
  REQUIRE(dict["window"].contains("__STATE__"));
  CHECK(dict["window"]["__STATE__"]["user"]["name"].as<std::string>() == "Ana");
  CHECK(dict["window"]["__STATE__"]["count"].as<int64_t>() == 3);
  REQUIRE(dict["window"]["__META__"].is_array());
  CHECK(dict["window"]["__META__"].size() == 2);
}

TEST_CASE("dict_steps cover flatten, filter, remove, rename, merge, sort, dedup",
          "[steps][dict]") {
  jsoncons::json dict = jsoncons::json::parse(R"({
    "meta": {"author": "Bob", "secret": "xyz"},
    "a": [1, 2],
    "b": "3",
    "items": [3, 1, 2, 1]
  })");
  std::vector<std::string> vals;

  scraper::RemoveKeysStep({"secret"}, false).apply(dummy_node(), vals, dict);
  CHECK_FALSE(dict["meta"].contains("secret"));

  scraper::RenameKeysStep({{"author", "writer"}}, false)
      .apply(dummy_node(), vals, dict);
  CHECK(dict["meta"]["writer"].as<std::string>() == "Bob");

  scraper::MergeKeysStep({"a", "b"}, "combined", false)
      .apply(dummy_node(), vals, dict);
  CHECK_FALSE(dict.contains("a"));
  CHECK_FALSE(dict.contains("b"));
  REQUIRE(dict["combined"].is_array());
  CHECK(dict["combined"].size() == 3);

  scraper::DeduplicateStep(false).apply(dummy_node(), vals, dict);
  scraper::SortKeysStep(true, false).apply(dummy_node(), vals, dict);
  REQUIRE(dict["items"] == jsoncons::json::parse("[1, 2, 3]"));

  scraper::FilterDictStep({"writer", "items"}, false)
      .apply(dummy_node(), vals, dict);
  CHECK(dict.contains("meta"));
  CHECK(dict.contains("items"));
  CHECK_FALSE(dict.contains("combined"));

  scraper::FlattenDictStep(false).apply(dummy_node(), vals, dict);
  CHECK(dict["meta_writer"].as<std::string>() == "Bob");
}


TEST_CASE("json_steps cover json_parse, json_dump, csv, binary, patch, pointer, and schema",
          "[steps][json]") {
  const std::string html = R"(
    <html><body>
      <script id="data">{"a": 1, /* comment */ "b": [10, 20,],}</script>
      <pre id="csv">name,qty
Widget,3
</pre>
    </body></html>
  )";

  scraper::ScraperBuilder sb;
  sb.Select("#data")
      .Text()
      .JsonParse("payload", {/*allow_comments=*/true,
                             /*allow_trailing_comma=*/true})
      .Select("#csv")
      .Text()
      .Trim()
      .CsvParse("rows")
      .JsonPathReplace("$.payload.b[1]", jsoncons::json(99))
      .JsonPointerSet("/payload/status", jsoncons::json("ok"))
      .MergePatch(jsoncons::json::parse(R"({"payload": {"a": 2}})"))
      .JsonPatch(jsoncons::json::parse(
          R"([{"op": "add", "path": "/patched", "value": true}])"))
      .JsonSchema(jsoncons::json::parse(R"({
        "type": "object",
        "required": ["payload", "rows", "patched"]
      })"));

  const auto out = sb.scrape_from_string(html);
  CHECK(out["payload"]["a"].as<int>() == 2);
  CHECK(out["payload"]["b"][1].as<int>() == 99);
  CHECK(out["payload"]["status"].as<std::string>() == "ok");
  CHECK(out["patched"].as<bool>() == true);
  REQUIRE(out["rows"].is_array());
  CHECK(out["rows"][0]["name"].as<std::string>() == "Widget");
  CHECK(out["rows"][0]["qty"].as<int>() == 3);

  // Non-strict json_schema records errors into error_key
  scraper::ScraperBuilder sb_nonstrict;
  sb_nonstrict.Select("#data")
      .Text()
      .JsonParse("payload", {/*allow_comments=*/true,
                             /*allow_trailing_comma=*/true})
      .JsonSchema(
          jsoncons::json::parse(
              R"({"type": "object", "required": ["missing_field"]})"),
          /*strict=*/false, "_errors");
  const auto out_err = sb_nonstrict.scrape_from_string(html);
  REQUIRE(out_err.contains("_errors"));
  CHECK(out_err["_errors"].is_array());
  CHECK_FALSE(out_err["_errors"].empty());

  // Binary round-trip + JsonDump
  jsoncons::json dict = jsoncons::json::parse(R"({"x": 7})");
  std::vector<std::string> vals;
  scraper::BinaryEncodeStep(xsel::query::BinaryFormat::Cbor,
                            xsel::query::BinaryEncoding::Base64)
      .apply(dummy_node(), vals, dict);
  REQUIRE(vals.size() == 1);
  jsoncons::json decoded_dict = jsoncons::json::object();
  scraper::BinaryDecodeStep(xsel::query::BinaryFormat::Cbor, "decoded",
                            xsel::query::BinaryEncoding::Base64)
      .apply(dummy_node(), vals, decoded_dict);
  CHECK(decoded_dict["decoded"]["x"].as<int>() == 7);

  scraper::JsonDumpStep(false, 2, "decoded")
      .apply(dummy_node(), vals, decoded_dict);
  REQUIRE(vals.size() == 1);
  CHECK(vals[0] == R"({"x":7})");

  // Lenient JS/JSON mode on JsonParseStep
  scraper::ScraperBuilder sb_lenient;
  sb_lenient.Select("#js")
      .Text()
      .JsonParse("js_data", {}, /*strict=*/true, {}, /*lenient=*/true);
  const auto out_lenient = sb_lenient.scrape_from_string(
      R"(<script id="js">{unquoted: 'hello' + ' world', flag: true, missing: undefined, trailing: [1, 2,],}</script>)");
  REQUIRE(out_lenient.contains("js_data"));
  CHECK(out_lenient["js_data"]["unquoted"].as<std::string>() == "hello world");
  CHECK(out_lenient["js_data"]["flag"].as<bool>() == true);
  CHECK(out_lenient["js_data"]["missing"].is_null());
  CHECK(out_lenient["js_data"]["trailing"].size() == 2);

  // Lenient mode on empty or whitespace/punctuation-only input does not throw
  // even with strict=true, and skips inserting an empty/null result when allow_empty=false.
  const auto out_lenient_empty = sb_lenient.scrape_from_string(
      R"(<script id="js">{}</script>)");
  CHECK(!out_lenient_empty.contains("js_data"));

  const auto out_lenient_blank = sb_lenient.scrape_from_string(
      R"(<script id="js">   </script>)");
  CHECK(!out_lenient_blank.contains("js_data"));
}

TEST_CASE("Edge cases: RegexExtractStep INT_MIN, SubstringStep, and FlattenDictStep depth",
          "[steps][edge]") {
  // INT_MIN group_index must not trigger signed overflow UB
  std::vector<std::string> vals = {"a=1"};
  jsoncons::json dict = jsoncons::json::object();
  scraper::RegexExtractStep("(\\w+)=(\\d+)", std::numeric_limits<int>::min())
      .apply(dummy_node(), vals, dict);
  CHECK(vals.empty());

  // SubstringStep negative start and negative length
  std::vector<std::string> sub_vals = {"abcdef"};
  scraper::SubstringStep(-4, -1).apply(dummy_node(), sub_vals, dict);
  REQUIRE(sub_vals.size() == 1);
  CHECK(sub_vals[0] == "cde");

  // FlattenDictStep enforces MAX_JSON_DEPTH on deeply nested arrays
  jsoncons::json deep = 1;
  for (int i = 0; i <= xsel::common::MAX_JSON_DEPTH + 2; ++i) {
    jsoncons::json arr = jsoncons::json::array();
    arr.push_back(std::move(deep));
    deep = std::move(arr);
  }
  CHECK_THROWS_AS(
      (void)scraper::FlattenDictStep(true).execute_postponed(deep),
      std::runtime_error);

  // BlockSeekerStep ignores braces and quotes inside JS comments
  std::vector<std::string> commented = {
      "window.__DATA__ = {\n"
      "  // comment with unmatched { and \"\n"
      "  /* block comment with } and ' */\n"
      "  ok: 42\n"
      "};"};
  scraper::BlockSeekerStep("window.__DATA__").apply(dummy_node(), commented, dict);
  REQUIRE(commented.size() == 1);
  const auto parsed_block = digger::parse_object(commented[0], true);
  CHECK(parsed_block["ok"].as<int64_t>() == 42);
}

TEST_CASE("Tier 1 steps: typed parsing, URLs, DOM, Aho-Corasick, hash/base64, ergonomics",
          "[steps][tier1]") {
  const std::string html = R"(
    <html><body>
      <div id="card" class="item featured" data-sku="SKU-42" data-id="99">
        <span class="price">$1,299.90</span>
        <span class="qty">7</span>
        <span class="stock">In Stock</span>
        <a class="link" href="/p/widget?id=42&utm_source=ads&ref=home">Buy <b>Now</b></a>
        <p class="desc">&lt;b&gt;Hello&lt;/b&gt; <i>World</i>!</p>
      </div>
      <table>
        <th class="k">Weight</th><td class="v">1.5kg</td>
        <th class="k">Color</th><td class="v">Black</td>
      </table>
    </body></html>
  )";

  // 1. Typed parsing + SetStep(type) + CastKeysStep
  scraper::ScraperBuilder sb_typed;
  sb_typed.Select(".price").Text().ParseNumber(".", ",", true, "price")
      .Select(".qty").Text().ToInt("qty")
      .Select(".stock").Text().ToBool("in_stock")
      .Select("#card").Attr("data-id").Set("card_id", {}, scraper::ValueType::Int);
  const auto typed_res = sb_typed.scrape_from_string(html);
  CHECK(std::abs(typed_res["price"].as<double>() - 1299.90) < 1e-6);
  CHECK(typed_res["qty"].as<int64_t>() == 7);
  CHECK(typed_res["in_stock"].as<bool>() == true);
  CHECK(typed_res["card_id"].as<int64_t>() == 99);

  // CastKeysStep
  jsoncons::json dict = jsoncons::json::parse(R"({"a": "12", "b": "3.5", "c": "no"})");
  dict = scraper::CastKeysStep(
             {{"a", scraper::ValueType::Int},
              {"b", scraper::ValueType::Float},
              {"c", scraper::ValueType::Bool}},
             true)
             .execute_postponed(dict);
  CHECK(dict["a"].as<int64_t>() == 12);
  CHECK(std::abs(dict["b"].as<double>() - 3.5) < 1e-6);
  CHECK(dict["c"].as<bool>() == false);

  // 2. URL steps + Slugify
  std::vector<std::string> urls = {"/p/widget?id=42&utm_source=ads&ref=home"};
  scraper::ResolveUrlStep("https://shop.example.com/cat/").apply(dummy_node(), urls, dict);
  REQUIRE(urls.size() == 1);
  CHECK(urls[0] == "https://shop.example.com/p/widget?id=42&utm_source=ads&ref=home");

  std::vector<std::string> host = urls;
  scraper::UrlPartStep("host").apply(dummy_node(), host, dict);
  CHECK(host[0] == "shop.example.com");

  std::vector<std::string> qid = urls;
  scraper::QueryParamStep("id").apply(dummy_node(), qid, dict);
  CHECK(qid[0] == "42");

  std::vector<std::string> stripped = urls;
  scraper::StripQueryStep({"id"}).apply(dummy_node(), stripped, dict);
  CHECK(stripped[0] == "https://shop.example.com/p/widget?id=42");

  std::vector<std::string> slugs = {"  Hello, World! 2026 -- Test "};
  scraper::SlugifyStep("-", 15).apply(dummy_node(), slugs, dict);
  CHECK(slugs[0] == "hello-world-202");

  // 3. DOM gaps: HtmlStep, TagNameStep, ClassesStep, AttrsStep, StripTagsStep
  scraper::ScraperBuilder sb_dom;
  sb_dom.Select("a.link").Html(false).Set("inner")
      .Select("a.link").Html(true).Set("outer")
      .Select("#card").TagName().Set("tag")
      .Select("#card").Classes().Set("classes")
      .Select("#card").Attrs(true);
  const auto dom_res = sb_dom.scrape_from_string(html);
  CHECK(dom_res["inner"].as<std::string>() == "Buy <b>Now</b>");
  CHECK(dom_res["outer"].as<std::string>().find("<a class=\"link\"") != std::string::npos);
  CHECK(dom_res["tag"].as<std::string>() == "div");
  REQUIRE(dom_res["classes"].is_array());
  CHECK(dom_res["classes"].size() == 2);
  CHECK(dom_res["data-sku"].as<std::string>() == "SKU-42");
  CHECK_FALSE(dom_res.contains("class"));

  std::vector<std::string> tagged = {"<p>Hello <b>World</b>!</p>"};
  scraper::StripTagsStep().apply(dummy_node(), tagged, dict);
  CHECK(tagged[0] == "Hello World!");

  // 4. Aho-Corasick replace_many / extract_many, Base64, Hash
  std::vector<std::string> rvals = {"foo bar baz foobar"};
  scraper::ReplaceManyStep({{"foo", "X"}, {"foobar", "Y"}, {"baz", "Z"}})
      .apply(dummy_node(), rvals, dict);
  CHECK(rvals[0] == "X bar Z Y");

  std::vector<std::string> evals = {"Supports USB-C, Bluetooth, and USB-C audio"};
  scraper::ExtractManyStep({"USB-C", "Bluetooth", "NFC"}, true)
      .apply(dummy_node(), evals, dict);
  REQUIRE(evals.size() == 2);
  CHECK(evals[0] == "USB-C");
  CHECK(evals[1] == "Bluetooth");

  std::vector<std::string> b64 = {"hello+world/?"};
  scraper::Base64EncodeStep(true).apply(dummy_node(), b64, dict);
  scraper::Base64DecodeStep(true).apply(dummy_node(), b64, dict);
  CHECK(b64[0] == "hello+world/?");

  std::vector<std::string> hashed = {"hello"};
  scraper::HashStep("sha256").apply(dummy_node(), hashed, dict);
  CHECK(hashed[0] == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");

  std::vector<std::string> fnv = {"hello"};
  scraper::HashStep("fnv1a64").apply(dummy_node(), fnv, dict);
  CHECK(fnv[0].size() == 16);

  // 5. Ergonomics: SortValues, ReverseValues, Coalesce, AppendTo, ZipKeys
  std::vector<std::string> nums = {"10", "2", "30"};
  scraper::SortValuesStep(true, true).apply(dummy_node(), nums, dict);
  CHECK(nums == std::vector<std::string>{"2", "10", "30"});
  scraper::ReverseValuesStep().apply(dummy_node(), nums, dict);
  CHECK(nums == std::vector<std::string>{"30", "10", "2"});

  std::vector<std::string> blanks = {"  ", "\t"};
  scraper::CoalesceStep("N/A").apply(dummy_node(), blanks, dict);
  CHECK(blanks == std::vector<std::string>{"N/A"});

  scraper::ScraperBuilder sb_erg;
  sb_erg.Select("#card")
      .Find(".price", scraper::QueryAllStep::Type::Text)
      .AppendTo("items")
      .Find(".qty", scraper::QueryAllStep::Type::Text)
      .AppendTo("items")
      .Select("table")
      .FindAll("th.k", scraper::QueryAllStep::Type::Text)
      .Set("keys")
      .FindAll("td.v", scraper::QueryAllStep::Type::Text)
      .Set("vals")
      .ZipKeys("keys", "vals", "specs");
  const auto erg_res = sb_erg.scrape_from_string(html);
  REQUIRE(erg_res["items"].is_array());
  CHECK(erg_res["items"].size() == 2);
  CHECK(erg_res["specs"]["Weight"].as<std::string>() == "1.5kg");
  CHECK(erg_res["specs"]["Color"].as<std::string>() == "Black");
}

TEST_CASE("Tier 2 steps: xpath, table, extract_meta, parse_date, unicode, fuzzy",
          "[steps][tier2]") {
  const std::string html = R"(
    <html>
      <head>
        <title>  Widget Pro  </title>
        <meta name="description" content="Best widget">
        <meta property="og:title" content="Widget Pro OG">
        <link rel="canonical" href="https://example.com/widget">
        <script type="application/ld+json">{"@type":"Product","name":"Widget Pro"}</script>
      </head>
      <body>
        <div id="catalog">
          <a href="/buy" data-id="99">Buy</a>
          <span class="dt">Nov 19, 2026</span>
          <span class="dt_iso">2026-11-19T12:30:00Z</span>
          <ul class="names">
            <li>Café Crème</li>
            <li>Crème Brûlée</li>
            <li>Espresso</li>
          </ul>
          <table id="specs">
            <thead><tr><th>Spec Name</th><th colspan="2">Value &amp; Unit</th></tr></thead>
            <tbody>
              <tr><td>Weight</td><td>1.2</td><td>kg</td></tr>
              <tr><td>Color</td><td>Matte</td><td>Black</td></tr>
            </tbody>
          </table>
        </div>
      </body>
    </html>
  )";

  scraper::ScraperBuilder sb;
  sb.Select("head")
      .ExtractMeta("meta")
      .Select("#catalog")
      .XPath(".//a/@data-id")
      .Set("buy_id")
      .Select("#specs")
      .Table("specs_table", true, true)
      .Select(".dt")
      .Text()
      .ParseDate("", "date", "release_date")
      .Select(".dt_iso")
      .Text()
      .ParseDate("", "unix", "release_unix")
      .Select(".names")
      .FindAll("li", scraper::QueryAllStep::Type::Text)
      .UnicodeNormalize("NFC")
      .StripAccents()
      .Set("ascii_names")
      .Select(".names")
      .FindAll("li", scraper::QueryAllStep::Type::Text)
      .StripAccents()
      .FuzzyFilter("Cafe Creme", 0.75, "jaro_winkler", false, true)
      .Set("fuzzy_matches")
      .Select(".names")
      .FindAll("li", scraper::QueryAllStep::Type::Text)
      .StripAccents()
      .Similarity("Espresso", "levenshtein", false, "sim_scores");

  const auto res = sb.scrape_from_string(html);

  CHECK(res["meta"]["title"].as<std::string>() == "Widget Pro");
  CHECK(res["meta"]["description"].as<std::string>() == "Best widget");
  CHECK(res["meta"]["og:title"].as<std::string>() == "Widget Pro OG");
  CHECK(res["meta"]["canonical"].as<std::string>() ==
        "https://example.com/widget");
  REQUIRE(res["meta"]["json_ld"].is_array());
  CHECK(res["meta"]["json_ld"][0]["name"].as<std::string>() == "Widget Pro");

  CHECK(res["buy_id"].as<std::string>() == "99");

  REQUIRE(res["specs_table"].is_array());
  REQUIRE(res["specs_table"].size() == 2);
  CHECK(res["specs_table"][0]["spec_name"].as<std::string>() == "Weight");
  CHECK(res["specs_table"][0]["value_unit"].as<std::string>() == "1.2");

  CHECK(res["release_date"].as<std::string>() == "2026-11-19");
  CHECK(res["release_unix"].is_int64());
  CHECK(res["release_unix"].as<int64_t>() == 1795091400);

  REQUIRE(res["ascii_names"].is_array());
  CHECK(res["ascii_names"][0].as<std::string>() == "Cafe Creme");
  CHECK(res["ascii_names"][1].as<std::string>() == "Creme Brulee");

  CHECK(res["fuzzy_matches"].as<std::string>() == "Cafe Creme");

  REQUIRE(res["sim_scores"].is_array());
  CHECK(std::abs(res["sim_scores"][2].as<double>() - 1.0) < 1e-6);
}

TEST_CASE("Steps edge cases: type coercion bounds, block comments, regex, URL, and dict depth",
          "[steps][edge]") {
  jsoncons::json dict = jsoncons::json::object();

  // 1. Out-of-range float to Int coercion must return null, not UB
  CHECK(scraper::coerce_string_to_json("1e30", scraper::ValueType::Int).is_null());
  CHECK(scraper::coerce_string_to_json("-1e30", scraper::ValueType::Int).is_null());
  CHECK(scraper::coerce_string_to_json("42.9", scraper::ValueType::Int).as<int64_t>() == 42);
  CHECK_THROWS_AS(scraper::parse_value_type_name("unknown_type"), std::runtime_error);

  // extract_number_string accounting parens, EU format, no digits
  CHECK(scraper::extract_number_string("(1,234.50)", ".", ",", true) == "-1234.50");
  CHECK(scraper::extract_number_string("1.234,56 EUR", ",", ".", true) == "1234.56");
  CHECK_FALSE(scraper::extract_number_string("no digits here", ".", ",", true).has_value());

  // 2. BlockSeekerStep skips braces inside comments/strings before the real block
  std::vector<std::string> blk = {
      R"(var cfg = /* {fake} */ " {also_fake} " + {real: 42};)"};
  scraper::BlockSeekerStep("var cfg =").apply(dummy_node(), blk, dict);
  REQUIRE(blk.size() == 1);
  CHECK(blk[0] == "{real: 42}");

  std::vector<std::string> unbal = {"var cfg = {unclosed: 1"};
  scraper::BlockSeekerStep("var cfg =").apply(dummy_node(), unbal, dict);
  CHECK(unbal.empty());

  // 3. Regex invalid pattern & FilterStep empty list
  CHECK_THROWS_AS(scraper::RegexReplaceStep("[unclosed", "x"), std::invalid_argument);
  CHECK_THROWS_AS(scraper::RegexFilterStep("[unclosed", false), std::invalid_argument);
  CHECK_THROWS_AS(scraper::RegexExtractStep("[unclosed", 1), std::invalid_argument);
  CHECK_THROWS_AS(scraper::FilterStep({}), std::runtime_error);

  // 4. URL edge cases
  CHECK_THROWS_AS(scraper::UrlPartStep("invalid_part"), std::runtime_error);
  std::vector<std::string> qp = {"https://example.com/search?q=hello+world%21&lang=en"};
  scraper::QueryParamStep("q", "none").apply(dummy_node(), qp, dict);
  CHECK(qp == std::vector<std::string>{"hello world!"});

  std::vector<std::string> slug = {"---!!!"};
  scraper::SlugifyStep("-").apply(dummy_node(), slug, dict);
  CHECK(slug == std::vector<std::string>{""});

  // 5. Dict depth guard on RemoveKeysStep / RenameKeysStep / FilterDictStep
  jsoncons::json deep = 1;
  for (int i = 0; i < xsel::common::MAX_JSON_DEPTH + 5; ++i) {
    jsoncons::json wrap = jsoncons::json::array();
    wrap.push_back(std::move(deep));
    deep = std::move(wrap);
  }
  CHECK_THROWS_AS(scraper::RemoveKeysStep({"x"}).execute_postponed(deep), std::runtime_error);
  CHECK_THROWS_AS(scraper::RenameKeysStep({{"a", "b"}}).execute_postponed(deep), std::runtime_error);
  CHECK_THROWS_AS(scraper::FilterDictStep({"a"}).execute_postponed(deep), std::runtime_error);
}

TEST_CASE("Hardening: SortValuesStep NaN/prefix, StripQuery UAF, XPath namespace, Unescape, extract_number, and path depth",
          "[steps][hardening]") {
  jsoncons::json dict = jsoncons::json::object();

  // 1. SortValuesStep(numeric=true) handles NaN, Inf, partial prefixes, whitespace, and ties safely
  std::vector<std::string> sort_vals = {"10", "NaN", "-nan", "inf", "10abc", " 2 ", "1.0", "1"};
  scraper::SortValuesStep(true, true).apply(dummy_node(), sort_vals, dict);
  REQUIRE(sort_vals.size() == 8);
  CHECK(sort_vals[0] == "1.0");
  CHECK(sort_vals[1] == "1");
  CHECK(sort_vals[2] == " 2 ");
  CHECK(sort_vals[3] == "10");

  // 2. StripQueryStep with keep_params must not read freed query memory
  std::vector<std::string> urls = {"https://example.com/search?a=1&b=2&c=3"};
  scraper::StripQueryStep({"b"}).apply(dummy_node(), urls, dict);
  CHECK(urls == std::vector<std::string>{"https://example.com/search?b=2"});

  // 3. XPathStep with XML_NAMESPACE_DECL nodes must not crash or trigger type confusion
  Document ns_doc;
  ns_doc.parseXmlMemory(R"(<root xmlns:ns="https://example.com/ns"><ns:item x="1">hi</ns:item></root>)");
  QueryResult ns_root(ns_doc.getRoot());
  REQUIRE(ns_root[0].has_value());
  std::vector<std::string> ns_vals;
  scraper::XPathStep("//namespace::*").apply(*ns_root[0], ns_vals, dict);
  CHECK_FALSE(ns_vals.empty());
  std::vector<std::string> ns_attr_vals;
  scraper::XPathStep("//namespace::*", "x").apply(*ns_root[0], ns_attr_vals, dict);
  CHECK(ns_attr_vals.empty());

  // 4. UnescapeStep handles \b and \f
  std::vector<std::string> esc_vals = {R"(a\bb\fc)"};
  scraper::UnescapeStep(false).apply(dummy_node(), esc_vals, dict);
  CHECK(esc_vals == std::vector<std::string>{"a\bb\fc"});

  // 5. extract_number_string handles unrelated parens and leading decimals
  CHECK(scraper::extract_number_string("(see note) $25.00") == "25.00");
  CHECK(scraper::extract_number_string("($25.00)") == "-25.00");
  CHECK(scraper::extract_number_string(".5") == "0.5");
  CHECK(scraper::extract_number_string("-.5") == "-0.5");

  // 6. Dotted path depth exceeding MAX_JSON_DEPTH throws
  std::string deep_key = "k";
  for (int i = 0; i < xsel::common::MAX_JSON_DEPTH + 5; ++i) {
    deep_key += ".k";
  }
  jsoncons::json deep_dest = jsoncons::json::object();
  CHECK_THROWS_AS(
      scraper::insert_into_json(deep_dest, deep_key, jsoncons::json(1)),
      std::runtime_error);
}
