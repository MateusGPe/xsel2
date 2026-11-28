"""Dict transforms, debug(), InsertMode/InsertOptions, Document, scrape_many."""

import tempfile
from pathlib import Path
import scraperpy as sp
from _helpers import load_topic

html, scrapers = load_topic(
    "dict_transforms_and_merging",
    ["dict_transforms", "dict_flatten_and_pick"],
)

# 1. Verify DSL dict transforms
d = {
    **scrapers["dict_ops"].from_string(html),
    **scrapers["flat_demo_scraper"].from_string(html),
}
item = d["item"]
assert item["specs"] == {"Switch": "Brown", "Size": "75%"}
assert item["price"] == 199.99
assert item["qty"] == 5
assert "headline" not in item
assert "fallback_title" not in item
assert item["tags"] == ["usb-c", "wireless"]
assert item["summary_parts"] == ["MK-1", "Mechanical Keyboard"]
assert d["flat_demo"] == {"deep_x": "valA"}

# 2. Verify Python ScraperPy parity: flatten, pick_keys, omit_keys, rename_keys,
#    merge_keys, zip_keys, coalesce_keys, cast_keys, sort_array, deduplicate, debug
py_dict = (
    sp.ScraperPy()
    .select("section.product")
    .find("h1.title", sp.Value.Text).set("a")
    .find("span.alt-title", sp.Value.Text).set("b")
    .find("span.qty-str", sp.Value.Text).set("qty")
    .find_all("ul.tags li", sp.Value.Text).set("tags")
    .find_all("dl.specs dt", sp.Value.Text).set("_k")
    .find_all("dl.specs dd", sp.Value.Text).set("_v")
    .zip_keys("_k", "_v", "specs")
    .omit_keys(["_k", "_v"])
    .cast_keys({"qty": "int"})
    .rename_keys({"a": "title"})
    .merge_keys(["title", "b"], "titles")
    .coalesce_keys({"primary": ["missing", "title"]}, drop_sources=False)
    .deduplicate()
    .sort_array(ascending=False)
    .pick_keys(["title", "qty", "specs", "tags", "titles", "primary"])
    .debug(prefix="EX08: ", print_dict=False, print_node=False)
    .from_string(html)
)
assert py_dict["qty"] == 5
assert py_dict["tags"] == ["wireless", "usb-c"]
assert py_dict["specs"]["Size"] == "75%"

# 3. Verify all 5 InsertMode values & InsertOptions (mode, allow_empty, nested_path)
opts_default = sp.InsertOptions()
opts_default.mode = sp.InsertMode.Default

opts_overwrite = sp.InsertOptions()
opts_overwrite.mode = sp.InsertMode.Overwrite
opts_overwrite.nested_path = True

opts_append = sp.InsertOptions()
opts_append.mode = sp.InsertMode.AppendToList
opts_append.allow_empty = False

opts_unique = sp.InsertOptions()
opts_unique.mode = sp.InsertMode.UniqueSuffix

opts_merge = sp.InsertOptions()
opts_merge.mode = sp.InsertMode.Merge

opts_flat = sp.InsertOptions()
opts_flat.nested_path = False

ins_res = (
    sp.ScraperPy()
    .select("section.product", "ins")
    .find("h1.title", sp.Value.Text).set("x", opts_default)
    .find("span.alt-title", sp.Value.Text).set("x", opts_unique)
    .find("h1.title", sp.Value.Text).set("over", opts_overwrite)
    .find("span.alt-title", sp.Value.Text).set("over", opts_overwrite)
    .find("h1.title", sp.Value.Text).set("arr", opts_append)
    .find("span.alt-title", sp.Value.Text).set("arr", opts_append)
    .find("h1.title", sp.Value.Text).set("a.b", opts_flat)
    .attrs(data_only=True, opts=opts_merge)
    .from_string(html)
)
assert ins_res["ins"]["x"] == "Keyboard"
assert ins_res["ins"]["x_0"] == "Mechanical Keyboard"
assert ins_res["ins"]["over"] == "Mechanical Keyboard"
assert ins_res["ins"]["arr"] == ["Keyboard", "Mechanical Keyboard"]
assert ins_res["ins"]["a.b"] == "Keyboard"
assert ins_res["ins"]["data-sku"] == "MK-1"

# 4. Verify Document.from_string, Document.from_file, from_document, from_file, scrape_many
doc_str = sp.Document.from_string(html)
s_doc = sp.ScraperPy().select("h1.title").text().set("title")
assert s_doc.from_document(doc_str) == {"title": "Keyboard"}

with tempfile.TemporaryDirectory() as td:
    tmp_html = Path(td) / "page.html"
    tmp_html.write_text(html, encoding="utf-8")
    doc_file = sp.Document.from_file(str(tmp_html))
    assert s_doc.from_document(doc_file) == {"title": "Keyboard"}
    assert s_doc.from_file(str(tmp_html)) == {"title": "Keyboard"}

many = sp.scrape_many({"one": s_doc, "two": scrapers["dict_ops"]}, html)
assert many["one"] == {"title": "Keyboard"}
assert many["two"]["item"]["qty"] == 5

print("ex_08_dict_transforms_and_merging: ALL ASSERTIONS PASSED")
