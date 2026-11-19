# C++20 API Guide

`xsel2` is organized into four static CMake libraries that you can embed directly in C++20 applications.

## CMake Targets

| Target | Headers | Purpose |
| --- | --- | --- |
| `xsel_common` | `common/ahocorasick.hpp`, `common/json_utils.hpp`, `common/utf.hpp` | Aho-Corasick matcher, UTF-8 & JSON helpers |
| `digger` | `digger/digger.hpp`, `digger/tokenizer.hpp`, `digger/scalar_parser.hpp` | Heuristic JS/JSON object extractor |
| `xsel_query` | `query/json_query.hpp` | JMESPath and JSONPath evaluation over `jsoncons::json` |
| `scraper` | `scraper/scraper.hpp`, `scraper/dsl_parser.hpp`, `scraper/merge.hpp` | Full scraping pipeline, steps, and `.dsl` parser |

Linking `scraper` transitively links `xsel_common`, `digger`, `xsel_query`, `jsoncons`, `mgp::lxml2-query`, `re2::re2`, and Abseil.

---

## `scraper::ScraperBuilder`

```cpp
#include "scraper/scraper.hpp"
#include <iostream>
#include <jsoncons/json.hpp>

int main() {
  scraper::ScraperBuilder builder;
  builder.Group("h1.product-title")
      .Text()
      .Normspace()
      .Set("title");

  jsoncons::json result = builder.scrape_from_string(
      "<html><body><h1 class=\"product-title\">  Hello   World  </h1></body></html>");

  std::cout << jsoncons::pretty_print(result) << '\n';
}
```

Use `builder.scrape_from_file("page.html")` to parse directly from disk.

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

## `xsel::query` (JMESPath & JSONPath)

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

---

## `AhoCorasick`

```cpp
#include "common/ahocorasick.hpp"
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
}
```
