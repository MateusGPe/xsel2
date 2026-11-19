"""Runnable example demonstrating scraperpy features on examples/sample.html."""

from pathlib import Path
import json
import scraperpy

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "sample.html").read_text(encoding="utf-8")

# 1. Fluent ScraperPy pipeline
s = scraperpy.ScraperPy()
(
    s.push("h1.product-title").text().normspace().set("title")
     .push(".price").attr("data-currency").set("currency")
     .push(".badge").text().trim().set("badges")
)
fluent_result = s.from_string(HTML)
print("Fluent ScraperPy result:")
print(json.dumps(fluent_result, indent=2))

# 2. Load and run scrapers from a .dsl file
scrapers = scraperpy.load_scrapers(str(ROOT / "product.dsl"))
dsl_result = scrapers["summary"].from_string(HTML)
print("\nDSL 'summary' scraper result:")
print(json.dumps(dsl_result, indent=2))

# 3. Aho-Corasick multi-pattern scan
ac = scraperpy.AhoCorasick()
for kw in ["Mechanical", "USB-C", "Bluetooth"]:
    ac.add_pattern(kw)
ac.build()
matches = ac.search(HTML)
print(f"\nAhoCorasick found {len(matches)} matches:")
for start, end, text, pat_id in matches:
    print(f"  [{start}:{end}] {text!r} (pattern_id={pat_id})")

# 4. Standalone JMESPath evaluation
all_badges = scraperpy.jmespath_eval(fluent_result, "[*].badges")
print(f"\nAll badges via JMESPath: {all_badges!r}")
