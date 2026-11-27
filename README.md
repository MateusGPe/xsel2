# xsel2

High-performance HTML/XML scraping, multi-pattern text search, and JSON transformation engine written in C++20, with the `xsel2` CLI, Python bindings (`scraperpy`), and the `xsel2` DSL (`.dsl`), a fluent pipeline language.

## 30-Second Before vs. After

Turn messy real-world HTML—whitespace-padded text, formatted prices, lazy `data-src` / relative URLs, dynamic `<dt>`/`<dd>` specs, and embedded `<script>` JS with unquoted keys, comments, and trailing commas—into clean, typed JSON in a 12-line `xsel2` (`.dsl`) file (no JS runtime required).

**Before (`card.html`)**

```html
<article class="product-card">
  <h1 class="title">  Mechanical   Keyboard   Pro  </h1>
  <span class="price">$1,299.50</span>
  <img class="hero" src="/img/placeholder.png" data-src="/img/mk-pro.webp" />
  <a class="manual" href="/docs/mk-pro.pdf">User Manual</a>
  <dl class="specs">
    <dt>Switch</dt><dd>Tactile Brown</dd>
    <dt>Layout</dt><dd>75% Compact</dd>
  </dl>
  <script>
    window.__STATE__ = {
      sku: "MK-75", // unquoted keys & comments
      rating: 4.8,
      inStock: true,
    };
  </script>
</article>
```

**After (`product.dsl` — 12 lines)**

```xsel2
let base = "https://shop.example"
product = Scraper()
  .$("article.product-card") {
    find("h1.title") | clean -> "title"
    | find(".price") | to_number(key="price")
    | find("img.hero", Attribute, ["data-src", "src"]) | url_join(base) -> "image"
    | find("a.manual", Attribute, "href") | url_join(base) -> "manual_url"
    | find_all("dl.specs dt") | clean -> "_k"
    | find_all("dl.specs dd") | clean -> "_v"
    | zip_keys("_k", "_v", "specs") | omit_keys(["_k", "_v"])
  }
  .$("script") { text | js_block("window.__STATE__ =") | parse_js("state") }
```

**Output (`xsel2 -d product.dsl -n product --html card.html`)**

```json
{
    "image": "https://shop.example/img/mk-pro.webp",
    "manual_url": "https://shop.example/docs/mk-pro.pdf",
    "price": 1299.5,
    "specs": {
        "Layout": "75% Compact",
        "Switch": "Tactile Brown"
    },
    "state": {
        "inStock": true,
        "rating": 4.8,
        "sku": "MK-75"
    },
    "title": "Mechanical Keyboard Pro"
}
```

See [`xsel2` DSL & Pipeline Steps](https://github.com/MateusGPe/xsel2/blob/main/docs/dsl-and-steps.md) for the full language and step reference.

## Features

- **HTML/XML Scraping & `xsel2` DSL**: CSS selector & XPath 1.0 / 3.1 extraction (`lxml2-query`, `libxml2`), HTML table & metadata extraction, fluent C++/Python pipelines, and an `xsel2` (`.dsl`) parser (`parse_dsl_string`, `load_scrapers`).
- **JS/JSON Digger**: Heuristically extracts and parses embedded JavaScript/JSON objects, comments, string concatenations, and typed scalars without executing JS.
- **Multi-Pattern & Fuzzy Algorithms**: Compressed-DFA Aho-Corasick matcher (`AhoCorasick`, `filter`, `replace`, `extract_many`) and UTF-8 Jaro-Winkler / Levenshtein fuzzy matching (`fuzzy_filter`, `similarity`).
- **JSON & CSV Queries**: JMESPath, JSONPath, JSON Schema, and CSV parsing/serialization powered by `jsoncons`.
- **Regex, Text, URL, Date & Unicode Steps**: RE2 regex filtering/replacement/extraction, RFC 3986 URL normalization, `absl::Time` date parsing, `utf8proc` Unicode normalization, and `FNV-1a 64`/`SHA-256`/Base64.

## Documentation

| Guide | Description |
| --- | --- |
| [Website & Interactive Docs](https://mateusgpe.github.io/xsel2) | Full documentation site and WebAssembly playground |
| [Quickstart](https://github.com/MateusGPe/xsel2/blob/main/docs/quickstart.md) | Prerequisites, CMake presets, helper scripts, building, testing, and 5-minute walkthrough |
| [CLI Reference](https://github.com/MateusGPe/xsel2/blob/main/docs/cli.md) | `xsel2` CLI (single input, `stdin` pipe, and multi-threaded `--batch`) |
| [Python Bindings (`scraperpy`)](https://github.com/MateusGPe/xsel2/blob/main/docs/python.md) | `ScraperPy`, `Document`, `scrape_many`, `InsertMode`, `AhoCorasick`, `.dsl` loading, JSON queries, and `pip`/`uv` packaging |
| [`xsel2` DSL & Pipeline Steps](https://github.com/MateusGPe/xsel2/blob/main/docs/dsl-and-steps.md) | Complete `xsel2` DSL (`.dsl`) manual: syntax, sugar, execution/merge model, caveats, control flow & composition, and pipeline steps reference |
| [C++20 API Guide](https://github.com/MateusGPe/xsel2/blob/main/docs/cpp-api.md) | Embedding `scraper`, `digger`, `xsel_query`, and `xsel_common` |
| [Exhaustive Examples Index](https://github.com/MateusGPe/xsel2/blob/main/examples/EXAMPLES_INDEX.md) | Complete coverage matrix of all DSL steps, `scraperpy` methods, and `xsel2` CLI flags (`ex_01`–`ex_09`) |
| [Examples](https://github.com/MateusGPe/xsel2/tree/main/examples) | Runnable [`sample.html`](https://github.com/MateusGPe/xsel2/blob/main/examples/sample.html), [`product.dsl`](https://github.com/MateusGPe/xsel2/blob/main/examples/product.dsl), [`scrape_example.py`](https://github.com/MateusGPe/xsel2/blob/main/examples/scrape_example.py), [`metadata.dsl`](https://github.com/MateusGPe/xsel2/blob/main/examples/metadata.dsl) / [`metadata_sample.html`](https://github.com/MateusGPe/xsel2/blob/main/examples/metadata_sample.html), [`marketplace.dsl`](https://github.com/MateusGPe/xsel2/blob/main/examples/marketplace.dsl) / [`marketplace_example.py`](https://github.com/MateusGPe/xsel2/blob/main/examples/marketplace_example.py), and complete all-steps showcase [`all_steps_showcase.dsl`](https://github.com/MateusGPe/xsel2/blob/main/examples/all_steps_showcase.dsl) / [`all_steps_showcase.py`](https://github.com/MateusGPe/xsel2/blob/main/examples/all_steps_showcase.py) |

## Quick Build & Test

Requires a C++20 compiler (GCC, Clang, or MSVC), CMake >= 3.24, and Python 3 dev headers. Dependencies (`jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, `utf8proc`, `Catch2`) are fetched automatically via [CPM.cmake](https://github.com/MateusGPe/xsel2/blob/main/cmake/CPM.cmake). `libxml2` uses the system package if found, and falls back to fetching `GNOME/libxml2` via CPM otherwise (controlled by `-DXSEL_FETCH_LIBXML2=AUTO|ON|OFF`). XPath 3.1 (`xpath3` / `XPath3`) is enabled by default via `-DXSEL_USE_XPATH3=ON|OFF`.

```bash
cmake --preset debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or run `./scripts/build_and_test.sh`. Use `--preset release` (`build-release/`) for an optimized `-O3` build. To build/install the `scraperpy` Python package via `scikit-build-core`, run `uv pip install .` (or `./scripts/pack_python.sh --clean --test`). See [Quickstart](https://github.com/MateusGPe/xsel2/blob/main/docs/quickstart.md) for install, helper scripts, and `asan-ubsan` details.

## Quick Examples

### CLI (`xsel2`)

```bash
# Quick CSS selector extraction
./build/xsel2 -s "h1.product-title" --html examples/sample.html

# Run a .dsl scraper
./build/xsel2 -d examples/product.dsl -n summary --html examples/sample.html

# Stream HTML through stdin (pipe mode)
cat examples/sample.html | ./build/xsel2 -d examples/product.dsl -n summary -c

# Multi-threaded batch scraping (NDJSON output)
./build/xsel2 -d examples/product.dsl -n summary --batch examples/*.html -j 0
```

### Python (`scraperpy`)

```bash
pip install scraperpy
```

Or run the repository examples against a local build:

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
PYTHONPATH=build:. python3 examples/marketplace_example.py
PYTHONPATH=build:. python3 examples/all_steps_showcase.py
```

```python
import scraperpy

s = scraperpy.ScraperPy()
s.select("h1").text().trim().set("title")
result = s.from_string("<html><body><h1>  Hello  </h1></body></html>")
```

Type stubs are provided in [`scraperpy-stubs/_scraperpy.pyi`](https://github.com/MateusGPe/xsel2/blob/main/scraperpy-stubs/_scraperpy.pyi).

## Project Structure

- `src/common/` — Aho-Corasick, UTF-8 utilities, JSON helpers
- `src/digger/` — Tokenizer, scalar parser, and JS/JSON object digger
- `src/query/` — JMESPath and JSONPath query engine
- `src/scraper/` — Extraction pipeline, merge logic, DSL parser, and pipeline steps
- `src/cli/` — `xsel2` executable
- `src/python/` — `pybind11` bindings (`_scraperpy`)
- `scraperpy/` & `scraperpy-stubs/` — Python wrapper, `py.typed`, and `.pyi` type stubs
- `scripts/` — Build, test, and Python wheel packaging helper scripts (`build.sh`, `test.sh`, `build_and_test.sh`, `pack_python.sh`)
- `docs/` & `examples/` — User guides, API references, and runnable examples
- `tests/` — Catch2 C++ unit test suite, CMake CLI tests, and `pytest` suites (`test_python.py`, `test_cli.py`)
