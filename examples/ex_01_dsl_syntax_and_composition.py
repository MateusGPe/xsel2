"""Example 01: DSL syntax, all 17 aliases, shorthands, control flow, and composition."""

import scraperpy as sp
from _helpers import EXAMPLES_DIR, load_pair

html, scrapers = load_pair("ex_01_dsl_syntax_and_composition")

# 1. Verify DSL syntax_and_aliases
res_syntax = scrapers["syntax_and_aliases"].from_string(html)
assert res_syntax["brand"] == "XSel2 Catalog"
assert res_syntax["env"] == "prod"
assert res_syntax["card_id"] == 101
assert res_syntax["card_name_upper"] == "ALPHA WIDGET"
assert res_syntax["card_name_lower"] == "alpha widget"

# 2. Verify control_flow (skip_if_exists, skip_if_empty, save/restore, tee, when_*, abort_if_exists)
res_cf = scrapers["control_flow"].from_string(html)
cards = res_cf["filtered_cards"]
assert isinstance(cards, dict) or len(cards) == 1
card_obj = cards[0] if isinstance(cards, list) else cards
assert card_obj["upper_name"] == "ALPHA WIDGET"
assert card_obj["lower_name"] == "alpha widget"
assert card_obj["badge"] == "Featured"
assert card_obj["matched_price"] == "USD $19.50"
assert res_cf["abort_demo"]["aborted_reason"] == "maintenance_mode"
assert "never_reached_after_abort" not in res_cf

# 3. Verify implicit_root + @key/@value collapsing
res_kv = scrapers["implicit_root"].from_string(html)
assert res_kv == {"Weight": "120g"}

# 4. Verify parse_dsl_string() and load_scrapers()
inline_dsl = """
pipe clean_title = { find("h1.brand", Text) | squish -> "title" }
quick = Scraper() | $("header.site-header") { use(clean_title) }
"""
inline_scrapers = sp.parse_dsl_string(inline_dsl)
assert inline_scrapers["quick"].from_string(html) == {"title": "XSel2 Catalog"}

reloaded = sp.load_scrapers(str(EXAMPLES_DIR / "ex_01_dsl_syntax_and_composition.dsl"))
assert "syntax_and_aliases" in reloaded

# 5. Python ScraperPy parity for composition & control-flow methods:
#    include, save, restore, skip_if_exists, skip_if_empty, abort_if_exists
base_py = sp.ScraperPy().select("header.site-header", "meta").find("h1.brand", sp.Value.Text).squish().set("brand")
full_py = (
    sp.ScraperPy()
    .include(base_py)
    .select("article.card", "cards")
    .skip_if_exists("span.skip-marker")
    .find("h2.name", sp.Value.Text)
    .skip_if_empty()
    .save("raw_name")
    .upper()
    .set("name_upper")
    .restore("raw_name")
    .lower()
    .set("name_lower")
    .select("div.abort-zone", "abort_zone")
    .abort_if_exists("span.maintenance", key="halted", reason="maint")
    .select("dl.specs", "after_abort")
    .find("dt", sp.Value.Text)
    .set("dt")
)
py_res = full_py.from_string(html)
assert py_res["meta"]["brand"] == "XSel2 Catalog"
assert py_res["cards"]["name_upper"] == "ALPHA WIDGET"
assert py_res["cards"]["name_lower"] == "alpha widget"
assert py_res["abort_zone"]["halted"] == "maint"
assert "after_abort" not in py_res

print("ex_01_dsl_syntax_and_composition: ALL ASSERTIONS PASSED")
