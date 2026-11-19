# Python Bindings (`scraperpy`)

`scraperpy` wraps the compiled `_scraperpy` C++20 extension module and ships type stubs in [`scraperpy-stubs/_scraperpy.pyi`](../scraperpy-stubs/_scraperpy.pyi).

## Importing `scraperpy`

When running from a local CMake build (without installing into site-packages), include both the build directory (containing `_scraperpy*.so`) and the repository root (containing `scraperpy/`) in `PYTHONPATH`:

```bash
PYTHONPATH=build:. python3 examples/scrape_example.py
```

## Exported Symbols

| Symbol | Kind | Purpose |
| --- | --- | --- |
| `ScraperPy` | Class | Fluent HTML/XML scraping & transformation pipeline |
| `Value` | Enum | Extraction target for `find` / `find_all` (`Text`, `OwnText`, `Attribute`, `ElementExists`, `AttributeExists`, `Adjacent`) |
| `Number` | Enum | Match selection for `find` (`All`, `Last`, `Nth`) |
| `InsertMode` | Enum | Collision handling when writing keys (`Default`, `Overwrite`, `AppendToList`, `UniqueSuffix`, `Merge`) |
| `InsertOptions` | Class | Controls `mode`, `allow_empty`, and `nested_path` on `set`, `count`, and `parse_js` |
| `AhoCorasick` | Class | Fast multi-pattern literal substring matcher |
| `jmespath_eval(data, expr)` | Function | Evaluates a JMESPath expression against Python dicts/lists |
| `jsonpath_query(data, expr)` | Function | Evaluates a JSONPath query against Python dicts/lists |
| `json_patch_apply(data, patch)` | Function | Applies an RFC 6902 JSON Patch to Python dicts/lists |
| `merge_patch_apply(data, patch)` | Function | Applies an RFC 7396 JSON Merge Patch to Python dicts/lists |
| `json_schema_validate(data, schema)` | Function | Validates Python data against a JSON Schema, returning `(is_valid, errors)` |
| `jsonpointer_get(data, pointer)` | Function | Extracts a value at an RFC 6901 JSON Pointer |
| `json_flatten(data)` / `json_unflatten(data)` | Function | Flattens or unflattens Python dicts/lists using JSONPath keys |
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

For the full list of pipeline methods and their parameters, see the [DSL & Pipeline Steps Reference](dsl-and-steps.md) (every DSL method maps 1:1 to a `ScraperPy` method).

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
```

---

## Multi-Pattern Search (`AhoCorasick`)

```python
import scraperpy

ac = scraperpy.AhoCorasick()
p0 = ac.add_pattern("Mechanical")
p1 = ac.add_pattern("USB-C")
ac.build()  # Compile the automaton before searching

if ac.contains_any("Mechanical Keyboard with USB-C"):
    # Returns List[Tuple[start, end, matched_text, pattern_id]]
    print(ac.search("Mechanical Keyboard with USB-C"))
```

---

## JSON Queries (`jmespath_eval` & `jsonpath_query`)

```python
import scraperpy

doc = {"items": [{"name": "A", "price": 5}, {"name": "B", "price": 20}]}
print(scraperpy.jmespath_eval(doc, "items[?price > `10`].name"))  # ['B']
print(scraperpy.jsonpath_query(doc, "$.items[*].name"))           # ['A', 'B']
```
