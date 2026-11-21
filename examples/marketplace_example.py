"""Comprehensive example showing synergy across xsel2 / scraperpy modules:
AhoCorasick pre-scan -> DSL scrapers (lenient digger, relaxed JSON, CSV,
JSONPath, JSON Schema, JMESPath) -> fluent ScraperPy ->
standalone JSON queries & validation.
"""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "marketplace_sample.html").read_text(encoding="utf-8")

# 1. Aho-Corasick pre-scan to detect embedded features in the HTML
ac = sp.AhoCorasick()
markers = [
    "window.__CATALOG_STATE__",
    "window.__PROMO_CONFIG__",
    "application/ld+json",
    "text/csv",
]
for marker in markers:
    ac.add_pattern(marker)
ac.build()

found_markers = {match[2] for match in ac.search(HTML)}
print("1. AhoCorasick detected features:", sorted(found_markers))
assert ac.has_all(HTML), "Expected all marketplace markers to be present"

# 2. Load and run DSL scrapers based on detected features
scrapers = sp.load_scrapers(str(ROOT / "marketplace.dsl"))

full_doc = scrapers["catalog_full"].from_string(HTML)
print("\n2. catalog_full (redacted, patched, schema-checked):")
print(json.dumps(full_doc, indent=2))

summary_doc = scrapers["catalog_summary"].from_string(HTML)
print("\n3. catalog_summary (JMESPath normalized):")
print(json.dumps(summary_doc, indent=2))

skus = scrapers["product_skus"].from_string(HTML)
print("\n4. product_skus (JSONPath):", skus)

# 3. Fluent ScraperPy pipeline extracting spec counts & USB-C products
fluent = (
    sp.ScraperPy()
    .select(".product-card", "cards")
    .attr("data-sku")
    .set("sku")
    .find_all("ul.specs li", sp.Value.Text)
    .squish()
    .regex_filter("USB-C")
    .count("usbc_spec_count")
)
fluent_res = fluent.from_string(HTML)
print("\n5. Fluent ScraperPy USB-C spec count:", fluent_res)

# 4. Standalone jsoncons queries & validation on scraped output
audio_names = sp.jsonpath_query(
    full_doc, "$.products[?(@.category == 'audio')].name"
)
promo_code = sp.jmespath_eval(full_doc, "window.__PROMO_CONFIG__.code")
print(
    f"\n6. Standalone queries -> audio_names={audio_names}, "
    f"promo_code={promo_code!r}"
)

summary_schema = {
    "type": "object",
    "required": ["store", "warehouse", "in_stock_products", "shipping_carriers"],
    "properties": {
        "store": {"type": "string"},
        "warehouse": {"type": "string"},
        "in_stock_products": {"type": "array", "minItems": 1},
        "shipping_carriers": {"type": "array"},
    },
}
ok, errors = sp.json_schema_validate(summary_doc, summary_schema)
print(f"7. Summary schema valid={ok}, errors={errors}")
