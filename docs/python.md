# Python Bindings (`scraperpy`)

`scraperpy` wraps the compiled `_scraperpy` C++20 extension module and ships type stubs in [`scraperpy-stubs/_scraperpy.pyi`](../scraperpy-stubs/_scraperpy.pyi).

## Importing `scraperpy`

When running from a local CMake build (without installing into site-packages), include both the build directory (containing `_scraperpy*.so`) and the repository root (containing `scraperpy/`) in `PYTHONPATH`:

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
```

## Building & Installing (`pip` / `uv` / Wheel Packaging)

`scraperpy` uses `pyproject.toml` with `scikit-build-core` to compile the `_scraperpy` C++20 extension and bundle `_scraperpy.pyi` type stubs plus `py.typed` directly inside the wheel:

```bash
# Install directly into the active Python environment
pip install .
# Or with uv:
uv pip install .

# Build sdist + wheels into dist/ and verify them with pytest
./scripts/pack_python.sh --clean --test
# Or build with uv / python -m build directly:
uv build
```

See `./scripts/pack_python.sh --help` for options (`--wheel-only`, `--sdist-only`, `-p "3.13 3.14"`, `-o <dir>`).

## Exported Symbols

| Symbol | Kind | Purpose |
| --- | --- | --- |
| `__version__` | String | Installed `scraperpy` / `_scraperpy` package version |
| `ScraperPy` | Class | Fluent HTML/XML scraping & transformation pipeline |
| `Document` | Class | Parsed HTML/XML DOM (`Document.from_string`, `Document.from_file`) for single-parse reuse |
| `scrape_many(scrapers, html)` | Function | Parses `html` once and executes every `ScraperPy` in `scrapers` on the shared `Document` |
| `Value` | Enum | Extraction target for `find` / `find_all` (`Text`, `OwnText`, `Attribute`, `ElementExists`, `AttributeExists`, `Adjacent`) |
| `InsertMode` | Enum | Collision handling when writing keys (`Default`, `Overwrite`, `AppendToList`, `UniqueSuffix`, `Merge`) |
| `InsertOptions` | Class | Controls `mode`, `allow_empty`, and `nested_path` on `set`, `count`, and `parse_js` |
| `AhoCorasick` | Class | Fast multi-pattern literal substring matcher |
| `jmespath_eval(data, expr)` | Function | Evaluates a JMESPath expression against Python dicts/lists |
| `jsonpath_query(data, expr)` | Function | Evaluates a JSONPath query against Python dicts/lists |
| `json_schema_validate(data, schema)` | Function | Validates Python data against a JSON Schema, returning `(is_valid, errors)` |
| `parse_js(source, simplify=True)` | Function | Heuristically parses JS/JSON-like text into Python objects without executing JS |
| `json_parse(text, ..., lenient=False, simplify=True)` | Function | Parses strict/relaxed JSON (or delegates to `parse_js` when `lenient=True`) |
| `parse_dsl_string(dsl_content)` | Function | Parses a `.dsl` string into `dict[str, ScraperPy]` |
| `load_scrapers(filepath)` | Function | Loads a `.dsl` file into `dict[str, ScraperPy]` |

---

## Fluent `ScraperPy` Pipelines

All builder methods return `self` (`ScraperPy`), so calls chain cleanly:

```python
import scraperpy

s = scraperpy.ScraperPy()
(
    s.select("h1.product-title").text().squish().set("title")
     .select(".price").text().regex_extract(r"([0-9]+\.[0-9]+)", 1).set("price")
     .select(".badge").text().trim().set("badges")
)

data = s.from_file("examples/sample.html")
# Or from a string:
# data = s.from_string(html_string)
```

For the full list of pipeline steps and their parameters, see the [DSL & Pipeline Steps Reference](dsl-and-steps.md). Keep these Python-specific notes in mind:

- **Full step coverage**: All registered pipeline steps (including `outer_html`, `rename_keys`, `merge_keys`, `find`, `find_all`, `extract_fields`, etc.) plus `include(other_scraper)` and the Python aliases `normalize_whitespace`, `extract_js_block`, `extract_js_blocks`, and `fields` are exposed as methods on `ScraperPy`.
- **DSL-only syntax & block constructs**: Top-level `let` / `pipe` + `use(...)`, shorthand operators (`$`, `@`, `??`, `[i]`, `->`, `>>`, `as`), block-scoped control flow (`tee { ... }` / `branch { ... }`, `each(...) { ... }`, `when_exists(...) { ... }`, `unless_exists(...) { ... }`, `when_match(...) { ... }`, `finalize { ... }`), and short DSL aliases (`strip`, `clean`, `sub`, `grep`, `extract`, `limit`, `dedup`, `json`, `to_number`, `url_join`, `inner_html`) are `.dsl` parser features. Use `parse_dsl_string` / `load_scrapers` to run block-scoped DSL constructs from Python, or call canonical method names directly on `ScraperPy`.
- **`InsertOptions` parameter order**: On `ScraperPy`, `set`, `count`, `parse_js`, `regex_groups`, `parse_number`, `parse_duration`, `attrs`, `table`, `extract_meta`, and `parse_date` accept an `opts: InsertOptions` parameter. Note that `ScraperPy.set(name, opts=..., type="string")` puts `opts` second, so pass `type` as a keyword argument in Python (e.g., `s.set("qty", type="int")`).

---

## Insertion Modes & Nested Paths (`InsertOptions`)

`set()`, `count()`, and `parse_js()` accept an optional `InsertOptions` instance:

```python
opts = scraperpy.InsertOptions()
opts.mode = scraperpy.InsertMode.AppendToList
opts.allow_empty = False
opts.nested_path = True  # "meta.title" -> {"meta": {"title": ...}}

s = scraperpy.ScraperPy()
s.select("h1").text().trim().set("meta.title", opts)
```

| `InsertMode` | Behavior on key collision |
| --- | --- |
| `Default` | `UniqueSuffix` for `set`/`count`; combine-as-array for `merge` |
| `Overwrite` | Replaces the existing value at the target key |
| `AppendToList` | Combines existing and new values into a list |
| `UniqueSuffix` | Generates `key_0`, `key_1`, ... on collision |
| `Merge` | Recursively merges dicts by key; combines non-dicts |

---

## Loading `.dsl` Files in Python

Keep scraping rules in `.dsl` files and execute them from Python:

```python
import scraperpy

scrapers = scraperpy.load_scrapers("examples/product.dsl")
result = scrapers["summary"].from_file("examples/sample.html")

# Or parse a DSL string directly:
inline = scraperpy.parse_dsl_string('title = Scraper().select("h1").text().trim().set("title")')
print(inline["title"].from_string("<h1>  Hi  </h1>"))

# Parse once and run multiple scrapers on the same DOM:
all_scrapers = scraperpy.load_scrapers("examples/marketplace.dsl")
results = scraperpy.scrape_many(all_scrapers, open("page.html").read())

# Or reuse an explicit Document across calls:
doc = scraperpy.Document.from_file("examples/sample.html")
summary = all_scrapers["catalog_summary"].from_document(doc)
```

---

## Multi-Pattern Search (`AhoCorasick`)

```python
import scraperpy

ac = scraperpy.AhoCorasick()
p0 = ac.add_pattern("Mechanical")
p1 = ac.add_pattern("USB-C")
ac.build()  # Compile the automaton before searching

text = "Mechanical Keyboard with USB-C"
if ac.contains_any(text) and ac.has_all(text):
    # Returns List[Tuple[start, end, matched_text, pattern_id]]
    print(f"patterns={ac.pattern_count()}, matches={ac.search(text)}")

ac.clear()  # Reset to add a new pattern set
```

---

## JSON Queries & Schema Validation

`scraperpy` exposes standalone `jsoncons`-backed helpers for Python dicts and lists:

```python
import scraperpy

doc = {"items": [{"name": "A", "price": 5}, {"name": "B", "price": 20}]}
print(scraperpy.jmespath_eval(doc, "items[?price > `10`].name"))  # ['B']
print(scraperpy.jsonpath_query(doc, "$.items[*].name"))           # ['A', 'B']

valid, errors = scraperpy.json_schema_validate(doc, {
    "type": "object",
    "required": ["items"],
    "properties": {"items": {"type": "array"}},
})
```

---

## Standalone JS & JSON Parsing (`parse_js`, `json_parse`)

Parse JS object literals or strict/relaxed JSON strings directly without building a `ScraperPy` pipeline:

```python
import scraperpy

# Strict JSON by default (raises RuntimeError on invalid JSON)
data = scraperpy.json_parse('{"a": 1, "b": [true, null]}')

# Relaxed JSON options (comments, trailing commas, etc.)
relaxed = scraperpy.json_parse('/* c */ {"a": 1,}', allow_comments=True, allow_trailing_comma=True)

# Lenient JS/JSON digger (unquoted keys, single quotes, undefined, '+' string concatenation)
js_obj = scraperpy.parse_js("{unquoted: 'a' + 'b', n: 42, extra: undefined,}")
# Equivalent via json_parse(..., lenient=True):
js_obj2 = scraperpy.json_parse("{unquoted: 'a' + 'b', n: 42, extra: undefined,}", lenient=True)

# Disable simplification (e.g. '+' concatenation) if raw token folding is not desired
raw = scraperpy.parse_js("{s: 'a' + 'b'}", simplify=False)  # {"s": "a"}
```

> **Note:** When `lenient=True` on `json_parse`, `jsoncons` options (`allow_comments`, `allow_trailing_comma`, `allow_inf_and_nan`, `lossless_number`, `max_nesting_depth`) are ignored, and empty or unparseable input returns `None` instead of raising `RuntimeError`.

For a full runnable demonstration of all `ScraperPy` steps, `InsertMode`s, `AhoCorasick`, and JSON utilities, see [`examples/all_steps_showcase.py`](../examples/all_steps_showcase.py).
