# xsel2

High-performance HTML/XML scraping, multi-pattern text search, and JSON transformation engine written in C++20, with CLI tools (`xmlsel-cli`, `xmlsel-pipe`) and Python bindings (`scraperpy`).

## Features

- **HTML/XML Scraping & DSL**: CSS selector extraction (`lxml2-query`) with fluent C++/Python pipelines and a `.dsl` parser (`parse_dsl_string`, `load_scrapers`).
- **JS/JSON Digger**: Extracts and parses embedded JavaScript/JSON objects, comments, string concatenations, and typed scalars.
- **Multi-Pattern Search**: Fast Aho-Corasick matcher (`AhoCorasick`) for scanning text against multiple patterns.
- **JSON Queries**: JMESPath and JSONPath evaluation (`jmespath_eval`, `jsonpath_query`) powered by `jsoncons`.
- **Regex & Text Steps**: RE2-backed regex filtering/replacement/extraction, string normalization, block parsing, and dictionary transformations.

## Requirements

- C++20 compiler (GCC or Clang)
- CMake >= 3.24
- `libxml2` development headers (used by `lxml2-query`)
- Python 3 development headers (for `_scraperpy` / `scraperpy`)
- Internet access on first configure (dependencies are fetched via [CPM.cmake](cmake/CPM.cmake): `jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, `Catch2`)

## Building & Testing

Using `CMakePresets.json`:

```bash
cmake --preset debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For an optimized Release build:

```bash
cmake --preset release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## CLI Usage

### `xmlsel-cli`

```bash
# Run a DSL scraper or CSS selector on an HTML file (or '-' for stdin)
xmlsel-cli scraper --dsl scrapers.dsl --name my_scraper --html page.html
xmlsel-cli scraper --selector "div.title" --html page.html --compact

# Multi-pattern search with Aho-Corasick
xmlsel-cli ahocorasick --patterns "foo,bar,baz" --text input.txt

# Evaluate JMESPath or JSONPath
xmlsel-cli jmespath --expr "items[*].name" --json data.json
xmlsel-cli jsonpath --expr "$.items[*].name" --json data.json --compact
```

### `xmlsel-pipe`

Reads HTML from `stdin` and writes extracted JSON to `stdout`:

```bash
cat page.html | xmlsel-pipe -d scrapers.dsl -n my_scraper -c
```

## Python Bindings (`scraperpy`)

The build produces the `_scraperpy` extension module. Type stubs are available in `scraperpy-stubs/_scraperpy.pyi`.

```python
import scraperpy

s = scraperpy.ScraperPy()
s.push("h1").text().trim().set("title")
result = s.from_string("<html><body><h1>  Hello  </h1></body></html>")
```

## Project Structure

- `src/common/` — Aho-Corasick, UTF-8 utilities, JSON helpers
- `src/digger/` — Tokenizer, scalar parser, and JS/JSON object digger
- `src/query/` — JMESPath and JSONPath query engine
- `src/scraper/` — Extraction pipeline, merge logic, DSL parser, and pipeline steps
- `src/cli/` — `xmlsel-cli` and `xmlsel-pipe` executables
- `src/python/` — `pybind11` bindings (`_scraperpy`)
- `scraperpy/` & `scraperpy-stubs/` — Python wrapper and `.pyi` type stubs
- `tests/` — Catch2 unit test suite
