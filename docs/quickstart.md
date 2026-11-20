# Quickstart

Get `xsel2` built, tested, and running in a few minutes.

## Prerequisites

- **C++20 compiler**: GCC 11+ or Clang 14+
- **CMake**: 3.24 or newer
- **libxml2 development headers (optional)**: e.g. `libxml2-dev` (Debian/Ubuntu) or `libxml2-devel` (Fedora/RHEL). If not installed on the system, CMake automatically falls back to fetching and building `GNOME/libxml2` via CPM (controlled by `-DXSEL_FETCH_LIBXML2=AUTO|ON|OFF`, default `AUTO`).
- **Python 3 development headers**: e.g. `python3-dev` (Debian/Ubuntu) or `python3-devel` (Fedora/RHEL), for `_scraperpy` / `scraperpy`
- **Internet access on first configure**: `cmake/CPM.cmake` automatically fetches `jsoncons`, `pybind11`, `lxml2-query`, `abseil-cpp`, `re2`, `Catch2`, and `libxml2` (when not found on the system).

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

Each preset sets `CMAKE_INSTALL_PREFIX` to `install/` in the repository root. To install `xsel2` and `_scraperpy`:

```bash
cmake --install build-release
```

## 5-Minute Walkthrough

The `examples/` directory includes a sample HTML page ([`examples/sample.html`](../examples/sample.html)), a `.dsl` scraper file ([`examples/product.dsl`](../examples/product.dsl)), and a Python script ([`examples/scrape_example.py`](../examples/scrape_example.py)).

### 1. Run a quick CSS selector from the CLI

```bash
./build/xsel2 -s "h1.product-title" --html examples/sample.html
```

### 2. Run a `.dsl` scraper with `xsel2`

```bash
# Run the "summary" scraper from examples/product.dsl
./build/xsel2 -d examples/product.dsl -n summary --html examples/sample.html

# Or stream HTML through stdin (pipe mode)
cat examples/sample.html | ./build/xsel2 -d examples/product.dsl -n summary -c
```

### 3. Run the Python example (`scraperpy`)

Add both the build directory (which contains `_scraperpy*.so`) and the repository root (which contains `scraperpy/`) to `PYTHONPATH`:

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
PYTHONPATH=build:. python3 examples/all_steps_showcase.py
```

## Next Steps

- [CLI Reference](cli.md) — `xsel2` options, pipe mode, and multi-threaded batch processing
- [Python Bindings (`scraperpy`)](python.md) — Fluent pipelines, `InsertMode`, `AhoCorasick`, and JSON queries
- [DSL & Pipeline Steps Reference](dsl-and-steps.md) — Complete `.dsl` syntax and step-by-step reference
- [C++ API Guide](cpp-api.md) — Linking `scraper`, `digger`, `xsel_query`, and `xsel_common`
