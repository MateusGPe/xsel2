"""JS digger, JSON/CSV parse & dump, JMESPath, JSONPath, JSON Schema."""

import json
import scraperpy as sp
from _helpers import load_topic

html, scrapers = load_topic(
    "js_digger_json_csv_queries",
    ["js_json_and_csv", "jsonpath_projection", "jmespath_projection"],
)

# 1. Verify DSL
d = scrapers["js_json_csv"].from_string(html)
assert d["js"]["data"]["sku"] == "XSEL-900"
assert d["js"]["data"]["note"] is None
assert d["js"]["data"]["payload"]["nested"] == [1, 2, 3]
assert d["js"]["extra"]["region"] == "us-east"
assert d["json_ops"]["cfg"]["service"] == "xsel2"
assert d["json_ops"]["lenient_obj"]["name"] == "Lenient"
assert len(d["csv_ops"]["rows"]) == 2
assert "Alice;95" in d["csv_ops"]["csv_semicolon"]
assert "_schema_errors" not in d

jmes = scrapers["jmespath_projection"].from_string(html)
assert jmes == {"sku": "XSEL-900", "price": 249.95, "nested": [1, 2, 3]}

qproj = scrapers["query_projections"].from_string(html)
assert qproj == ["Alice"]

# 2. Verify Python ScraperPy parity
py_res = (
    sp.ScraperPy()
    .select("script#hydration", "js")
    .text().js_block("window.__DATA__ =").parse_js("data", simplify=True)
    .text().extract_js_block("window.__EXTRA__ =").parse_js("extra", simplify=True)
    .text().js_blocks(["window.__EXTRA__ ="])
    .text().extract_js_blocks({"window.__EXTRA__ =": "ex_alias"})
    .select("div.json-commented", "j")
    .text().json_parse("cfg", allow_comments=True, allow_trailing_comma=True)
    .json_dump(pretty=False, source_key="cfg").set("compact_json")
    .select("pre.csv-block", "c")
    .text().trim().csv_parse("rows", assume_header=True, delimiter=",", trim=True, strict=True)
    .csv_dump(assume_header=True, delimiter=",", trim=True, source_key="rows").set("csv_out")
    .jmespath("{js: js, j: j, rows: c.rows, count: length(c.rows)}")
    .json_schema({"type": "object", "required": ["rows", "count"]}, strict=True)
    .from_string(html)
)
assert py_res["js"]["data"]["sku"] == "XSEL-900"
assert json.loads(py_res["j"]["compact_json"])["version"] == 2
assert py_res["count"] == 2

py_jp = (
    sp.ScraperPy()
    .select("pre.csv-block")
    .text().trim().csv_parse("rows")
    .jsonpath("$.rows[*].name")
    .from_string(html)
)
assert py_jp == ["Alice", "Bob"]

# 3. Verify standalone Python module functions:
#    parse_js, json_parse, jmespath_eval, jsonpath_query, json_schema_validate
js_obj = sp.parse_js('{ a: 1, b: "x" + "y", }', simplify=True)
assert js_obj == {"a": 1, "b": "xy"}

j_obj = sp.json_parse('/* c */ {"x": 10,}', allow_comments=True, allow_trailing_comma=True)
assert j_obj == {"x": 10}

assert sp.jmespath_eval({"items": [{"id": 1}, {"id": 2}]}, "items[*].id") == [1, 2]
assert sp.jsonpath_query({"items": [{"id": 1}, {"id": 2}]}, "$.items[1].id") == [2]

ok, errs = sp.json_schema_validate({"id": 1}, {"type": "object", "required": ["id"]})
assert ok and errs == []
bad_ok, bad_errs = sp.json_schema_validate({}, {"type": "object", "required": ["id"]})
assert not bad_ok and len(bad_errs) > 0

print("ex_07_js_digger_json_csv_queries: ALL ASSERTIONS PASSED")
