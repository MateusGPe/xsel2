# Quickstart

Get `xsel2` built, tested, and running in a few minutes.

## Prerequisites

- **C++20 compiler**: GCC 11+ or Clang 14+
- **CMake**: 3.24 or newer
- **libxml2 development headers**: e.g. `libxml2-dev` (Debian/Ubuntu) or `libxml2-devel` (Fedora/RHEL)
- **Python 3 development headers**: e.g. `python3-dev` (Debian/Ubuntu) or `python3-devel` (Fedora/RHEL), for `_scraperpy` / `scraperpy`
- **Internet access on first configure**: `cmake/CPM.cmake` automatically fetches `jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, and `Catch2`.

## Build & Test

`CMakePresets.json` provides three ready-to-use configure presets:

| Preset | Compiler | Build Directory | Purpose |
| --- | --- | --- | --- |
| `debug` | System default | `build/` | Development & unit tests |
| `release` | System default | `build-release/` | Optimized (`-O3`) binaries & bindings |
| `asan-ubsan` | Clang (`clang` / `clang++`) | `build-asan/` | AddressSanitizer + UndefinedBehaviorSanitizer |

Build and run the Catch2 test suite with the `debug` preset:

```bash
cmake --preset debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For an optimized release build:

```bash
cmake --preset release
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

### Installing Binaries (Optional)

Each preset sets `CMAKE_INSTALL_PREFIX` to `install/` in the repository root. To install `xmlsel-cli`, `xmlsel-pipe`, and `_scraperpy`:

```bash
cmake --install build-release
```

## 5-Minute Walkthrough

The `examples/` directory includes a sample HTML page ([`examples/sample.html`](../examples/sample.html)), a `.dsl` scraper file ([`examples/product.dsl`](../examples/product.dsl)), and a Python script ([`examples/scrape_example.py`](../examples/scrape_example.py)).

### 1. Run a quick CSS selector from the CLI

```bash
./build/xmlsel-cli scraper --selector "h1.product-title" --html examples/sample.html
```

### 2. Run a `.dsl` scraper with `xmlsel-cli` or `xmlsel-pipe`

```bash
# Run the "summary" scraper from examples/product.dsl
./build/xmlsel-cli scraper --dsl examples/product.dsl --name summary --html examples/sample.html

# Or stream HTML through xmlsel-pipe
cat examples/sample.html | ./build/xmlsel-pipe -d examples/product.dsl -n summary -c
```

### 3. Run the Python example (`scraperpy`)

Add both the build directory (which contains `_scraperpy*.so`) and the repository root (which contains `scraperpy/`) to `PYTHONPATH`:

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
```

## Next Steps

- [CLI Reference](cli.md) — All `xmlsel-cli` subcommands and `xmlsel-pipe` options
- [Python Bindings (`scraperpy`)](python.md) — Fluent pipelines, `InsertMode`, `AhoCorasick`, and JSON queries
- [DSL & Pipeline Steps Reference](dsl-and-steps.md) — Complete `.dsl` syntax and step-by-step reference
- [C++ API Guide](cpp-api.md) — Linking `scraper`, `digger`, `xsel_query`, and `xsel_common`
