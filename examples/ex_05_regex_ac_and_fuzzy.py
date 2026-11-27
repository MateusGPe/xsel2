"""Example 05: RE2 regex, Aho-Corasick (pipeline + class), and fuzzy matching."""

import scraperpy as sp
from _helpers import load_pair

html, scrapers = load_pair("ex_05_regex_ac_and_fuzzy")

# 1. Verify DSL
d = scrapers["regex_ac_fuzzy"].from_string(html)
assert d["regex"]["order_num"] == "4920"
assert d["regex"]["ord_code"] == "AB"
assert d["regex"]["ord_num"] == "4920"
assert d["regex"]["all_prices"] == ["12.50", "99.00", "7.25"]
assert "MATCHED" in d["regex"]["dotall_replaced"]
assert len(d["regex"]["multiline_lines"]) == 3
assert len(d["regex"]["non_refurb"]) == 3
assert d["ac"]["filtered_codes"] == ["SKU-100-PRO", "SKU-400-PRO"]
assert d["ac"]["tech_unique"] == ["C++20", "RE2", "Aho-Corasick"]
assert d["ac"]["tech_all"] == ["C++20", "RE2", "C++20"]
assert "Mechanical Keyboard" in d["fuzzy"]["jw_matches"]
assert "Mechnical Keybord" in d["fuzzy"]["lev_matches"]
assert len(d["fuzzy"]["jw_scores"]) == 4

# 2. Verify Python ScraperPy parity
py_res = (
    sp.ScraperPy()
    .select("section.regex-box", "re")
    .find("h1.title", sp.Value.Text).regex_replace("#", "No.").set("rep")
    .find("h1.title", sp.Value.Text).regex_extract("([0-9]+)", 1).set("num")
    .find("h1.title", sp.Value.Text).regex_groups("(?P<id>[0-9]+)", prefix="g_").set("unused")
    .find("p.prices", sp.Value.Text).regex_find_all("([0-9]+\\.[0-9]{2})", 1).set("prices")
    .find_all("ul.codes li", sp.Value.Text).regex_filter("REFURB", negate=True).set("kept")
    .find_all("ul.codes li", sp.Value.Text).filter(["PRO"]).set("ac_filt")
    .find("div.notes", sp.Value.Text).extract_many(["C++20", "RE2"], unique=True).set("ac_ext")
    .select("ul.fuzzy-list", "fz")
    .find_all("li", sp.Value.Text).fuzzy_filter("Mechanical Keyboard", threshold=0.75, algo="jaro_winkler", case_sensitive=False, sort_by_score=True).set("jw")
    .find_all("li", sp.Value.Text).similarity("Mechanical Keyboard", algo="levenshtein", case_sensitive=False, key="lev_scores")
    .from_string(html)
)
assert py_res["re"]["num"] == "4920"
assert py_res["re"]["g_id"] == "4920"
assert py_res["re"]["ac_filt"] == ["SKU-100-PRO", "SKU-400-PRO"]
assert len(py_res["fz"]["lev_scores"]) == 4

# 3. Verify Python AhoCorasick class (add_pattern, build, pattern_count, contains_any, has_all, search, clear)
ac = sp.AhoCorasick()
p0 = ac.add_pattern("C++20")
p1 = ac.add_pattern("RE2")
assert p0 == 0 and p1 == 1
ac.build()
assert ac.pattern_count() == 2
assert ac.contains_any("Built with C++20")
assert ac.has_all("Built with C++20 and RE2")
assert not ac.has_all("Only C++20")
hits = ac.search("Using C++20 with RE2")
assert len(hits) == 2
assert hits[0][2] == "C++20" and hits[1][2] == "RE2"
ac.clear()
assert ac.pattern_count() == 0

print("ex_05_regex_ac_and_fuzzy: ALL ASSERTIONS PASSED")
