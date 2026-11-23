"""Runnable example demonstrating Article & JSON-LD schema scraping."""

from pathlib import Path
import json
import scraperpy as sp

ROOT = Path(__file__).resolve().parent
HTML = (ROOT / "article_news.html").read_text(encoding="utf-8")

# 1. Run DSL pipeline
scrapers = sp.load_scrapers(str(ROOT / "article_news.dsl"))
article_doc = scrapers["article"].from_string(HTML)
print("=== Article & JSON-LD Output (DSL) ===")
print(json.dumps(article_doc, indent=2))

assert article_doc["title"] == "Building Ultra-Fast Native Web Scrapers with C++20"
assert article_doc["author"] == "Sarah Connor"
assert article_doc["json_ld"]["@type"] == "TechArticle"
assert article_doc["json_ld"]["wordCount"] == 1420
assert "Technology" in article_doc["breadcrumbs"]
assert "#cpp" in article_doc["tags"]

# 2. Standalone JMESPath extraction
headline = sp.jmespath_eval(article_doc, "json_ld.headline")
assert headline == article_doc["title"]

print("\nAll article & JSON-LD checks passed!")
