# C++20 API Guide

`xsel2` is organized into four static CMake libraries that you can embed directly in C++20 applications.

## CMake Targets

| Target | Headers | Purpose |
| --- | --- | --- |
| `xsel_common` | `common/ahocorasick.hpp`, `common/fuzzy.hpp`, `common/hex_utils.hpp`, `common/json_utils.hpp`, `common/string_utils.hpp`, `common/utf.hpp` | Aho-Corasick matcher, Jaro-Winkler & Levenshtein fuzzy matching, hex/string/UTF-8 & JSON helpers |
| `digger` | `digger/digger.hpp`, `digger/tokenizer.hpp`, `digger/scalar_parser.hpp` | Heuristic JS/JSON object extractor |
| `xsel_query` | `query/json_query.hpp` | JMESPath, JSONPath, JSON Schema & CSV over `jsoncons::json` |
| `scraper` | `scraper/scraper.hpp`, `scraper/dsl_parser.hpp`, `scraper/merge.hpp` | Full scraping pipeline, steps, and `xsel2` (`.dsl`) parser |

Linking `scraper` transitively links `xsel_common`, `digger`, `xsel_query`, `jsoncons`, `mgp::lxml2-query`, `re2::re2`, `utf8proc`, and Abseil.

---

## `scraper::ScraperBuilder`

```cpp
#include "scraper/scraper.hpp"
#include <iostream>
#include <jsoncons/json.hpp>

int main() {
  scraper::ScraperBuilder builder;
  builder.Select("h1.product-title")
      .Text()
      .Squish()
      .Set("title");

  jsoncons::json result = builder.scrape_from_string(
      "<html><body><h1 class=\"product-title\">  Hello   World  </h1></body></html>");

  std::cout << jsoncons::pretty_print(result) << '\n';
}
```

Use `builder.scrape_from_file("page.html")` to parse directly from disk.

### Error Policies & `InsertOptions`

`Scraper` supports three error policies via `set_error_policy(scraper::ErrorPolicy)`:
- `ErrorPolicy::LogAndContinue` (default): logs extractor errors to `error_stream` and continues.
- `ErrorPolicy::ThrowOnError`: rethrows the first step exception immediately.
- `ErrorPolicy::CollectErrors`: appends formatted errors to `scraper.errors()` without printing.

`InsertOptions` (`mode`: `Default`, `Overwrite`, `AppendToList`, `UniqueSuffix`, `Merge`; `allow_empty`; `nested_path`) is accepted by `Set`, `Count`, `ParseJs`, `ExtractFields`, `RegexGroups`, `JsonParse`, `CsvParse`, `ParseNumber`, `ParseDuration`, `ToInt`, `ToFloat`, `ToBool`, `Attrs`, `Table`, `ExtractMeta`, `ParseDate`, and `QueryParams`.

### Single-Parse `Document` Reuse, Single-Pass Extraction & Composition

Parse HTML once into `Document` (`lxml2-query.h`), run zero-alloc single-pass extraction (`Find`, `ExtractFields`), and compose pipelines with `Each`, `Tee`, `WhenExists`/`UnlessExists`/`WhenMatch`, `SkipIfExists`/`SkipIfEmpty`/`AbortIfExists`, `Save`/`Restore`, `Finalize`, and `Include`:

```cpp
#include "scraper/scraper.hpp"
#include <iostream>

int main() {
  scraper::FieldSpec title_spec;
  title_spec.key = "title";
  title_spec.selector_str = "h1";

  scraper::FieldSpec badge_spec;
  badge_spec.key = "has_sale";
  badge_spec.selector_str = ".sale-badge";
  badge_spec.type = scraper::QueryAllStep::Type::ElementExists;

  scraper::ScraperBuilder sub;
  sub.Select("")
      .Text()
      .Squish()
      .Set("text");

  scraper::ScraperBuilder main_scraper;
  main_scraper.Select("body")
      .ExtractFields({title_spec, badge_spec})
      .Find(".views")
      .ParseNumber(".", ",", true, "views", {}, /*compact=*/true)
      .Each("ul.tags li", "tags", sub.get_extractors().front()->steps())
      .Tee({std::make_shared<scraper::CountValuesStep>("tag_nodes")})
      .SkipIfExists(".deleted");

  // Parse once and share Document across multiple scrapers:
  Document doc;
  doc.parseMemory(
      "<html><body><h1>  Keyboard  </h1><span class='views'>1.2K</span>"
      "<ul class='tags'><li>mech</li></ul></body></html>");

  jsoncons::json out = jsoncons::json::object();
  main_scraper.scrape_document(doc, out, "page");
  std::cout << jsoncons::pretty_print(out) << '\n';
}
```

Use `builder.Include(other_scraper)` to append extractors and `finalize_steps` from another `Scraper`, and `builder.Finalize(steps)` to attach post-processing steps that always run once after all extractors finish.

---

## `scraper::dsl::DslParser`

```cpp
#include "scraper/dsl_parser.hpp"
#include <iostream>

int main() {
  scraper::dsl::DslParser parser;
  auto scrapers = parser.parse_file("examples/product.dsl");

  auto it = scrapers.find("summary");
  if (it != scrapers.end()) {
    jsoncons::json result = it->second.scrape_from_file("examples/sample.html");
    std::cout << jsoncons::pretty_print(result) << '\n';
  }
}
```

You can also parse an in-memory string with `parser.parse(dsl_source)`.

---

## `digger::parse_object`

Extracts a typed `jsoncons::json` tree from a JavaScript or JSON-like `std::string_view` without executing JS:

```cpp
#include "digger/digger.hpp"
#include <iostream>

int main() {
  std::string_view js = R"({ sku: "MK-PRO-75", rating: 4.8, inStock: true, note: "a" + "b" })";
  jsoncons::json obj = digger::parse_object(js, /*simplify=*/true);
  std::cout << jsoncons::pretty_print(obj) << '\n';
}
```

---

## `xsel::query` (JMESPath, JSONPath & Schema)

```cpp
#include "query/json_query.hpp"
#include <iostream>

int main() {
  auto doc = jsoncons::json::parse(R"({"items":[{"name":"A","price":5},{"name":"B","price":20}]})");
  auto jmes = xsel::query::evaluate_jmespath(doc, "items[?price > `10`].name");
  auto jpath = xsel::query::evaluate_jsonpath(doc, "$.items[*].name");
  std::cout << jmes << '\n' << jpath << '\n';
}
```

Also available in `query/json_query.hpp`:
- `parse_json`, `dump_json` (`JsonParseOptions`)
- `validate_json_schema`
- `decode_csv`, `encode_csv` (`CsvOptions`)

---

## `AhoCorasick` & Fuzzy Matching (`xsel_common`)

```cpp
#include "common/ahocorasick.hpp"
#include "common/fuzzy.hpp"
#include <iostream>

int main() {
  AhoCorasick ac;
  ac.AddPattern("Mechanical");
  ac.AddPattern("USB-C");
  ac.Compile();

  AhoCorasick::MatchList matches;
  ac.Find("Mechanical Keyboard with USB-C", matches);
  for (const auto &m : matches) {
    std::cout << "pattern=" << m.pattern_id << " start=" << m.start << " len=" << m.length << '\n';
  }

  double jw = xsel::common::jaro_winkler_similarity("Mechanical Keyboard", "Mechnical Keybord");
  double lev = xsel::common::levenshtein_similarity("Mechanical Keyboard", "Mechnical Keybord");
  std::cout << "jw=" << jw << " lev=" << lev << '\n';
}
```
