"""Example 06: List slicing/ordering, typed set, parse_number, parse_duration, to_*, parse_date."""

import scraperpy as sp
from _helpers import load_pair

html, scrapers = load_pair("ex_06_slicing_ordering_and_types")

# 1. Verify DSL
d = scrapers["slicing_and_types"].from_string(html)
assert d["lists"]["sorted_num"] == ["2", "10", "30"]
assert d["lists"]["first_2"] == ["10", "2"]
assert d["lists"]["last_2"] == ["30", "2"]
assert d["lists"]["nth_2"] == "30"
assert d["lists"]["slice_1_2"] == ["2", "30"]
assert d["lists"]["unique_count"] == 3
assert d["lists"]["typed_int"] == 42
assert d["lists"]["typed_float"] == 3.14
assert d["lists"]["typed_bool"] is True

assert d["parsed_nums"]["us"] == 1299.5
assert d["parsed_nums"]["eu"] == 1299.5
assert d["parsed_nums"]["views"] == 1500000
assert d["parsed_nums"]["i"] == 42
assert d["parsed_nums"]["i_fallback"] == -1
assert d["parsed_nums"]["f"] == 3.14
assert d["parsed_nums"]["b"] is True

assert d["durations"]["iso_sec"] == 5415
assert d["durations"]["human_min"] == 135
assert d["durations"]["clock_ms"] == 3723000

assert d["dates"]["date_only"] == "2026-05-10"
assert d["dates"]["custom"] == "2026-05-10"
assert d["dates"]["rel_date"] == "2026-05-08"
assert d["fallback"]["val"] == "N/A"

# 2. Verify Python ScraperPy parity
py_res = (
    sp.ScraperPy()
    .select("section.numbers", "py")
    .find_all("ul.scores li", sp.Value.Text).unique(True).sort(ascending=True, numeric=True).set("s")
    .find_all("ul.scores li", sp.Value.Text).reverse().first(2).set("rev2")
    .find_all("ul.scores li", sp.Value.Text).last(1).set("last1")
    .find_all("ul.scores li", sp.Value.Text).nth(1).set("nth1")
    .find_all("ul.scores li", sp.Value.Text).slice(0, 1).set("sl")
    .find_all("ul.scores li", sp.Value.Text).count("cnt")
    .find("span.int-str", sp.Value.Text).append_to("arr", allow_empty=False)
    .find("span.us-price", sp.Value.Text).parse_number(decimal=".", thousands=",", allow_float=True, key="price", compact=False)
    .find("span.int-str", sp.Value.Text).to_int(key="i")
    .find("span.float-str", sp.Value.Text).to_float(key="f")
    .find("span.bool-str", sp.Value.Text).to_bool(key="b")
    .select("section.durations", "dur")
    .find("span.iso-dur", sp.Value.Text).parse_duration(key="sec", unit="seconds")
    .select("section.dates", "dt")
    .find("span.relative", sp.Value.Text).parse_date(output="date", tz="UTC", now="2026-05-10T00:00:00Z", key="rel")
    .select("div.empty-fallback", "fb")
    .text().default("empty").set("val")
    .from_string(html)
)
assert py_res["py"]["s"] == ["2", "10", "30"]
assert py_res["py"]["cnt"] == 4
assert py_res["py"]["arr"] == ["42"]
assert py_res["py"]["price"] == 1299.5
assert py_res["dur"]["sec"] == 5415
assert py_res["dt"]["rel"] == "2026-05-08"
assert py_res["fb"]["val"] == "empty"

print("ex_06_slicing_ordering_and_types: ALL ASSERTIONS PASSED")
