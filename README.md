# xsel2

High-performance HTML/XML scraping, multi-pattern text search, and JSON transformation engine written in C++20, with CLI tools (`xmlsel-cli`, `xmlsel-pipe`), Python bindings (`scraperpy`), and a fluent `.dsl` pipeline language.

## Features

- **HTML/XML Scraping & DSL**: CSS selector extraction (`lxml2-query`) with fluent C++/Python pipelines and a `.dsl` parser (`parse_dsl_string`, `load_scrapers`).
- **JS/JSON Digger**: Heuristically extracts and parses embedded JavaScript/JSON objects, comments, string concatenations, and typed scalars without executing JS.
- **Multi-Pattern Search**: Compressed-DFA Aho-Corasick matcher (`AhoCorasick`) for scanning text against many literal patterns in a single pass.
- **JSON Queries**: JMESPath and JSONPath evaluation (`jmespath_eval`, `jsonpath_query`) powered by `jsoncons`.
- **Regex & Text Steps**: RE2-backed regex filtering/replacement/extraction, string normalization, block parsing, and dictionary transformations.

## Documentation

| Guide | Description |
| --- | --- |
| [Quickstart](docs/quickstart.md) | Prerequisites, CMake presets, building, testing, and 5-minute walkthrough |
| [CLI Reference](docs/cli.md) | `xmlsel-cli` (`scraper`, `ahocorasick`, `jmespath`, `jsonpath`) and `xmlsel-pipe` |
| [Python Bindings (`scraperpy`)](docs/python.md) | `ScraperPy`, `InsertMode`, `AhoCorasick`, `.dsl` loading, and JSON queries |
| [DSL & Pipeline Steps](docs/dsl-and-steps.md) | Complete `.dsl` syntax, execution model, and all pipeline steps |
| [C++20 API Guide](docs/cpp-api.md) | Embedding `scraper`, `digger`, `xsel_query`, and `xsel_common` |
| [Examples](examples/) | Runnable [`sample.html`](examples/sample.html), [`product.dsl`](examples/product.dsl), and [`scrape_example.py`](examples/scrape_example.py) |

## Quick Build & Test

Requires a C++20 compiler (GCC or Clang), CMake >= 3.24, `libxml2` dev headers, and Python 3 dev headers. Dependencies (`jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, `Catch2`) are fetched automatically via [CPM.cmake](cmake/CPM.cmake).

```bash
cmake --preset debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Use `--preset release` (`build-release/`) for an optimized `-O3` build. See [Quickstart](docs/quickstart.md) for install and `asan-ubsan` details.

## Quick Examples

### CLI (`xmlsel-cli` & `xmlsel-pipe`)

```bash
# Quick CSS selector extraction
./build/xmlsel-cli scraper --selector "h1.product-title" --html examples/sample.html

# Run a .dsl scraper
./build/xmlsel-cli scraper --dsl examples/product.dsl --name summary --html examples/sample.html

# Stream HTML through stdin with xmlsel-pipe
cat examples/sample.html | ./build/xmlsel-pipe -d examples/product.dsl -n summary -c

# Multi-pattern search & JSON queries
./build/xmlsel-cli ahocorasick --patterns "Mechanical,USB-C,Bluetooth" --text examples/sample.html
./build/xmlsel-cli jmespath --expr "items[*].name" --json '{"items":[{"name":"A"},{"name":"B"}]}' --compact
```

### Python (`scraperpy`)

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
```

```python
import scraperpy

s = scraperpy.ScraperPy()
s.select("h1").text().trim().set("title")
result = s.from_string("<html><body><h1>  Hello  </h1></body></html>")
```

Type stubs are provided in [`scraperpy-stubs/_scraperpy.pyi`](scraperpy-stubs/_scraperpy.pyi).

## Project Structure

- `src/common/` — Aho-Corasick, UTF-8 utilities, JSON helpers
- `src/digger/` — Tokenizer, scalar parser, and JS/JSON object digger
- `src/query/` — JMESPath and JSONPath query engine
- `src/scraper/` — Extraction pipeline, merge logic, DSL parser, and pipeline steps
- `src/cli/` — `xmlsel-cli` and `xmlsel-pipe` executables
- `src/python/` — `pybind11` bindings (`_scraperpy`)
- `scraperpy/` & `scraperpy-stubs/` — Python wrapper and `.pyi` type stubs
- `docs/` & `examples/` — User guides, API references, and runnable examples
- `tests/` — Catch2 unit test suite
