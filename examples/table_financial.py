"""Runnable example demonstrating HTML table extraction & JMESPath transformations."""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "table_financial.html").read_text(encoding="utf-8")

# 1. Run DSL pipeline
scrapers = sp.load_scrapers(str(ROOT / "table_financial.dsl"))
summary = scrapers["market_summary"].from_string(HTML)
print("=== Financial Table Summary (DSL) ===")
print(json.dumps(summary, indent=2))
assert summary["index"] == "Tech & Semiconductor Index"
assert len(summary["buy_recommendations"]) == 3
assert summary["buy_recommendations"][0]["symbol"] == "NVDA"

# 2. Fluent Python extraction of specific table cells
fluent = (
    sp.ScraperPy()
    .select("table.financial-matrix tbody tr td.symbol", "tickers")
    .text()
    .squish()
    .set("symbols")
)
fluent_res = fluent.from_string(HTML)
print("\n=== All Tickers via Fluent ScraperPy ===")
print(fluent_res)
tickers = [item["symbols"] for item in fluent_res["tickers"]]
assert "NVDA" in tickers
print("\nAll financial table checks passed!")
