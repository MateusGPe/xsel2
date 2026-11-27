"""Example 02: DOM selection, all Value modes, extract_fields, XPath 1.0/3.1, tables, metadata."""

import json
import scraperpy as sp
from _helpers import load_pair

html, scrapers = load_pair("ex_02_dom_and_selection")

# 1. Verify DSL dom_and_values
d1 = scrapers["dom_and_values"].from_string(html)
box = d1["box"]
assert box["tag"] == "section"
assert box["classes"] == ["panel", "primary-panel"]
assert box["data-id"] == "42"
assert box["data-role"] == "showcase"
assert box["full_text"] == "Direct text nested span tail."
assert box["own_text"] == "Direct text tail."
assert box["href"] == "/docs/guide"
assert box["link_exists"] == "true"
assert box["has_data_alt"] == "true"
assert box["adjacent_text"] == "Next B"
assert d1["html_inspect"]["inner"] == "Hello <b>World</b>!"
assert d1["html_inspect"]["outer"] == '<div class="rich">Hello <b>World</b>!</div>'
assert d1["text_modes"]["text_own"] == "Direct text tail."
assert d1["attr_list"]["first_found_attr"] == "/docs/alt"
assert d1["fields_demo"]["doc_href"] == "/docs/guide"
assert d1["fields_demo"]["missing_opt"] == "N/A"

# 2. Verify DSL xpath_tables_meta
d2 = scrapers["xpath_tables_meta"].from_string(html)
assert d2["meta"]["title"] == "DOM Showcase — xsel2"
assert d2["meta"]["canonical"] == "https://example.com/dom-showcase"
assert d2["meta"]["json_ld_by_type"]["Product"]["sku"] == "DP-01"
assert len(d2["pricing_rows"]) == 2
assert d2["pricing_rows"][0]["plan_name"] == "Starter"
assert d2["raw_rows"] == [["Row1-A", "Row1-B"], ["Row2-A", "Row2-B"]]
assert d2["xpath_demo"]["xp1_texts"] == ["One", "Two", "Three"]
assert d2["xpath_demo"]["xp1_attrs"] == ["1", "2", "3"]
assert d2["xpath_demo"]["xp3_joined"] == "One, Two, Three"
assert d2["xpath_demo"]["xp3_attrs"] == ["1", "2", "3"]

# XPath 3.1 check via fluent ScraperPy (enabled by default with -DXSEL_USE_XPATH3=ON)
xp3_res = (
    sp.ScraperPy()
    .select("ul.xml-items", "xp3")
    .xpath3("string-join(.//li/text(), ', ')")
    .set("joined")
    .from_string(html)
)
assert xp3_res["xp3"]["joined"] == "One, Two, Three"

# 3. Fluent ScraperPy parity covering every DOM/XPath/Table/Meta method & Value enum
py_res = (
    sp.ScraperPy()
    .extract_meta("meta", include_json_ld=True, index_json_ld=True)
    .select("section#dom-box", "dom")
    .tag_name()
    .set("tag")
    .classes()
    .set("classes")
    .attrs(data_only=False)
    .find("p.intro", sp.Value.Text, squish=True, required=False)
    .set("v_text")
    .find("p.intro", sp.Value.OwnText, squish=True)
    .set("v_own")
    .find("a.doc-link", sp.Value.Attribute, ["data-alt", "href"])
    .set("v_attr")
    .find("a.doc-link", sp.Value.ElementExists)
    .set("v_exists")
    .find("a.doc-link", sp.Value.AttributeExists, "href")
    .set("v_attr_exists")
    .find_all("ul.xml-items li", sp.Value.Text, squish=True)
    .set("li_all")
    .extract_fields({"h": {"sel": "p.intro", "type": "own_text", "squish": True}})
    .fields({"doc": {"sel": "a.doc-link", "type": "attr", "attr": "href"}})
    .select("span.label", "dom")
    .find("span.next-label", sp.Value.Adjacent)
    .set("v_adj")
    .select("div.rich", "rich")
    .text(own_text=False)
    .set("text")
    .html(outer=False)
    .set("inner")
    .outer_html()
    .set("outer")
    .select("a.doc-link", "link")
    .attr("href")
    .set("href")
    .select("table.pricing", "tbl")
    .table(key="rows", header=True, normalize_headers=True, trim_cells=True)
    .table(header=False, trim_cells=True)
    .set("rows_json")
    .select("ul.xml-items", "xp")
    .xpath(".//li/text()")
    .set("xp1")
    .from_string(html)
)
assert py_res["dom"]["v_exists"] == "true"
assert py_res["dom"]["v_attr_exists"] == "true"
assert py_res["dom"]["v_adj"] == "Next B"
assert py_res["dom"]["doc"] == "/docs/guide"
assert py_res["rich"]["inner"] == "Hello <b>World</b>!"
assert py_res["link"]["href"] == "/docs/guide"
assert len(json.loads(py_res["tbl"]["rows_json"])) == 3
assert py_res["xp"]["xp1"] == ["One", "Two", "Three"]

print("ex_02_dom_and_selection: ALL ASSERTIONS PASSED")
