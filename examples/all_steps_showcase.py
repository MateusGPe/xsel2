"""Comprehensive showcase of xsel2 / scraperpy steps, features, and algorithms."""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "all_steps_showcase.html").read_text(encoding="utf-8")

# ---------------------------------------------------------------------------
# 1. Run all DSL showcase scrapers via load_scrapers()
# ---------------------------------------------------------------------------
scrapers = sp.load_scrapers(str(ROOT / "all_steps_showcase.dsl"))
for name in [
    "dom_and_text",
    "urls_dates_unicode",
    "algorithms_and_matching",
    "types_tables_and_dict",
    "serialization_and_binary",
    "json_transforms_and_patch",
]:
    res = scrapers[name].from_string(HTML)
    print(f"=== DSL[{name}] ===")
    print(json.dumps(res, indent=2, ensure_ascii=False)[:400] + "\n...")

# Verify key outputs from DSL scrapers
algo_res = scrapers["algorithms_and_matching"].from_string(HTML)
assert "Mechanical Keyboard" in algo_res["fuzzy"]["jw_matches"]
assert "Mechnical Keybord" in algo_res["fuzzy"]["lev_matches"]

ser_res = scrapers["serialization_and_binary"].from_string(HTML)
for fmt in ["cbor_decoded", "msgpack_decoded", "bson_decoded", "ubjson_decoded"]:
    assert ser_res["binary_roundtrip"][fmt] == {"env": "staging", "retries": 3}

# ---------------------------------------------------------------------------
# 2. Inline DSL parsing via parse_dsl_string()
# ---------------------------------------------------------------------------
inline = sp.parse_dsl_string(
    'quick = Scraper() | $("h1.main-title") { text | squish | strip_accents | slugify() -> "slug" }'
)
inline_res = inline["quick"].from_string(HTML)
print("\n=== parse_dsl_string ===")
print(inline_res)
assert inline_res["slug"] == "cafe-resume-mechanical-gear"

# ---------------------------------------------------------------------------
# 3. Fluent ScraperPy: Value, Number, InsertMode & InsertOptions
# ---------------------------------------------------------------------------
opts_overwrite = sp.InsertOptions()
opts_overwrite.mode = sp.InsertMode.Overwrite
opts_overwrite.nested_path = True

opts_append = sp.InsertOptions()
opts_append.mode = sp.InsertMode.AppendToList
opts_append.allow_empty = False

opts_unique = sp.InsertOptions()
opts_unique.mode = sp.InsertMode.UniqueSuffix

opts_merge = sp.InsertOptions()
opts_merge.mode = sp.InsertMode.Merge

opts_flat_key = sp.InsertOptions()
opts_flat_key.nested_path = False

fluent = (
    sp.ScraperPy()
    # Nested path + Overwrite + UniqueSuffix + AppendToList + flat key in "hero"
    .select("header#hero", "hero")
    .find("h1.main-title", sp.Value.Text)
    .squish()
    .set("meta.title", opts_overwrite)
    .tag_name()
    .set("tag", opts_unique)
    .classes()
    .set("tag", opts_unique)
    .attr("data-region")
    .set("hero.region", opts_flat_key)
    .attr("data-region")
    .set("tags_list", opts_append)
    .attr("data-tier")
    .set("tags_list", opts_append)
    # Number.Nth & Number.Last in "catalog"
    .select("section#catalog", "catalog")
    .find("article.item h2.title", sp.Value.Text, "", sp.Number.Nth, 0)
    .set("first_product")
    .find("article.item h2.title", sp.Value.Text, "", sp.Number.Last)
    .set("last_product")
    # Fuzzy similarity in Python
    .select("ul.models", "fuzzy")
    .find_all("li", sp.Value.Text)
    .fuzzy_filter("Mechanical Keyboard", threshold=0.8, algo="jaro_winkler", sort_by_score=True)
    .set("jw")
)
fluent_res = fluent.from_string(HTML)
print("\n=== Fluent ScraperPy (InsertMode, Value, Number, Fuzzy) ===")
print(json.dumps(fluent_res, indent=2, ensure_ascii=False))
assert fluent_res["hero"]["meta"]["title"] == "Café Résümé & Mechanical Gear"
assert fluent_res["hero"]["hero.region"] == "na-east"
assert fluent_res["hero"]["tag"] == "header"
assert fluent_res["hero"]["tag_0"] == ["banner", "featured-banner"]
assert fluent_res["hero"]["tags_list"] == ["na-east", "pro"]
assert fluent_res["catalog"]["first_product"] == "Mechanical Keyboard Pro 75"
assert fluent_res["catalog"]["last_product"] == "Thunderbolt USB-C Dock"

# ---------------------------------------------------------------------------
# 4. AhoCorasick multi-pattern automaton
# ---------------------------------------------------------------------------
ac = sp.AhoCorasick()
for kw in ["Mechanical", "USB-C", "Bluetooth", "Thunderbolt"]:
    ac.add_pattern(kw)
ac.build()

assert ac.pattern_count() == 4
assert ac.contains_any(HTML)
assert ac.has_all(HTML)
matches = ac.search("USB-C and Bluetooth on a Mechanical Keyboard")
print("\n=== AhoCorasick ===")
print("pattern_count:", ac.pattern_count(), "matches:", matches)
ac.clear()
assert ac.pattern_count() == 0

# ---------------------------------------------------------------------------
# 5. Standalone JSON query, pointer, patch, flatten & schema functions
# ---------------------------------------------------------------------------
doc = {
    "store": "xsel2",
    "items": [
        {"sku": "MK-75", "price": 1299.5, "in_stock": True},
        {"sku": "MS-02", "price": 89.0, "in_stock": False},
    ],
}

in_stock_skus = sp.jmespath_eval(doc, "items[?in_stock].sku")
all_prices = sp.jsonpath_query(doc, "$.items[*].price")
first_sku = sp.jsonpointer_get(doc, "/items/0/sku")

patched = sp.json_patch_apply(
    doc, [{"op": "replace", "path": "/items/1/in_stock", "value": True}]
)
merged = sp.merge_patch_apply(patched, {"store": "xsel2-prod", "currency": "USD"})

flattened = sp.json_flatten(merged)
unflattened = sp.json_unflatten(flattened)
assert unflattened == merged

schema = {
    "type": "object",
    "required": ["store", "currency", "items"],
    "properties": {
        "store": {"type": "string"},
        "currency": {"type": "string"},
        "items": {"type": "array", "minItems": 2},
    },
}
valid, errors = sp.json_schema_validate(merged, schema)
print("\n=== Standalone JSON Tools ===")
print("jmespath:", in_stock_skus, "jsonpath:", all_prices, "jsonpointer:", first_sku)
print("schema valid:", valid, "errors:", errors)
assert valid and not errors
print("\nAll showcase checks passed!")
