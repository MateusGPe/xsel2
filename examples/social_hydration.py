"""Runnable example demonstrating Social Media & JS Hydration scraping."""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "social_hydration.html").read_text(encoding="utf-8")

# 1. Run DSL pipeline
scrapers = sp.load_scrapers(str(ROOT / "social_hydration.dsl"))
social_doc = scrapers["social"].from_string(HTML)
print("=== Social Media & Hydration Output (DSL) ===")
print(json.dumps(social_doc, indent=2))

assert social_doc["author"] == "linus_torvalds"
assert social_doc["followers"] == 1250000
assert social_doc["post"] == "Talk is cheap. Show me the code."
assert social_doc["likes"] == 14820
assert len(social_doc["top_comments"]) == 2
assert social_doc["top_comments"][0]["user"] == "code_ninja"

# 2. Standalone relaxed JS digger test
digger_state = sp.parse_js(HTML.split("window.__INITIAL_DATA__ =")[1].split(";</script>")[0])
assert digger_state["threadId"] == "status_9921"

print("\nAll social hydration checks passed!")
