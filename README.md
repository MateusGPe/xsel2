# xsel2

High-performance HTML/XML scraping, multi-pattern text search, and JSON transformation engine written in C++20, with the `xsel2` CLI, Python bindings (`scraperpy`), and a fluent `.dsl` pipeline language.

## Features

- **HTML/XML Scraping & DSL**: CSS selector & XPath 1.0 extraction (`lxml2-query`, `libxml2`), HTML table & metadata extraction, fluent C++/Python pipelines, and a `.dsl` parser (`parse_dsl_string`, `load_scrapers`).
- **JS/JSON Digger**: Heuristically extracts and parses embedded JavaScript/JSON objects, comments, string concatenations, and typed scalars without executing JS.
- **Multi-Pattern & Fuzzy Algorithms**: Compressed-DFA Aho-Corasick matcher (`AhoCorasick`, `filter`, `replace`, `extract_many`) and UTF-8 Jaro-Winkler / Levenshtein fuzzy matching (`fuzzy_filter`, `similarity`).
- **JSON & CSV Queries**: JMESPath, JSONPath, JSON Schema, and CSV parsing/serialization powered by `jsoncons`.
- **Regex, Text, URL, Date & Unicode Steps**: RE2 regex filtering/replacement/extraction, RFC 3986 URL normalization, `absl::Time` date parsing, `utf8proc` Unicode normalization, and `FNV-1a 64`/`SHA-256`/Base64.

## Documentation

| Guide | Description |
| --- | --- |
| [Quickstart](docs/quickstart.md) | Prerequisites, CMake presets, helper scripts, building, testing, and 5-minute walkthrough |
| [CLI Reference](docs/cli.md) | `xsel2` CLI (single input, `stdin` pipe, and multi-threaded `--batch`) |
| [Python Bindings (`scraperpy`)](docs/python.md) | `ScraperPy`, `Document`, `scrape_many`, `InsertMode`, `AhoCorasick`, `.dsl` loading, JSON queries, and `pip`/`uv` packaging |
| [DSL & Pipeline Steps](docs/dsl-and-steps.md) | Complete `.dsl` manual: syntax, sugar, execution/merge model, caveats, control flow & composition, and pipeline steps reference |
| [C++20 API Guide](docs/cpp-api.md) | Embedding `scraper`, `digger`, `xsel_query`, and `xsel_common` |
| [Examples](examples/) | Runnable [`sample.html`](examples/sample.html), [`product.dsl`](examples/product.dsl), [`scrape_example.py`](examples/scrape_example.py), [`metadata.dsl`](examples/metadata.dsl) / [`metadata_sample.html`](examples/metadata_sample.html), [`marketplace.dsl`](examples/marketplace.dsl) / [`marketplace_example.py`](examples/marketplace_example.py), and complete all-steps showcase [`all_steps_showcase.dsl`](examples/all_steps_showcase.dsl) / [`all_steps_showcase.py`](examples/all_steps_showcase.py) |

## Quick Build & Test

Requires a C++20 compiler (GCC or Clang), CMake >= 3.24, and Python 3 dev headers. Dependencies (`jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, `utf8proc`, `Catch2`) are fetched automatically via [CPM.cmake](cmake/CPM.cmake). `libxml2` uses the system package if found, and falls back to fetching `GNOME/libxml2` via CPM otherwise (controlled by `-DXSEL_FETCH_LIBXML2=AUTO|ON|OFF`).

```bash
cmake --preset debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or run `./scripts/build_and_test.sh`. Use `--preset release` (`build-release/`) for an optimized `-O3` build. To build/install the `scraperpy` Python package via `scikit-build-core`, run `uv pip install .` (or `./scripts/pack_python.sh --clean --test`). See [Quickstart](docs/quickstart.md) for install, helper scripts, and `asan-ubsan` details.

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

Type stubs are provided in [`scraperpy-stubs/_scraperpy.pyi`](scraperpy-stubs/_scraperpy.pyi).

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
