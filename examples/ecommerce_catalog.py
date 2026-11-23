"""Runnable example demonstrating E-Commerce Catalog & Variant scraping."""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "ecommerce_catalog.html").read_text(encoding="utf-8")

# 1. Run DSL pipeline
scrapers = sp.load_scrapers(str(ROOT / "ecommerce_catalog.dsl"))
catalog_doc = scrapers["catalog"].from_string(HTML)
print("=== Catalog & Multi-Variant Output (DSL) ===")
print(json.dumps(catalog_doc, indent=2))

assert len(catalog_doc["items"]) == 2
first_item = catalog_doc["items"][0]
assert first_item["name"] == "Titanium Wireless Earbuds"
assert first_item["price_usd"] == 129.0
assert first_item["available"] is True
assert "Space Gray" in first_item["available_colors"]
assert catalog_doc["next_page_url"] == "https://audiohub.example/catalog?page=2"

# 2. Standalone JMESPath query for in-stock products
in_stock = sp.jmespath_eval(catalog_doc, "items[?available == `true`].name")
assert in_stock == ["Titanium Wireless Earbuds"]

print("\nAll catalog & variant checks passed!")
