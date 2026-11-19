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
