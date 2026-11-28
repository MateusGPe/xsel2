"""Text normalization, replace, split/join, Base64, hashing, Unicode, map_values."""

import scraperpy as sp
from _helpers import load_topic

html, scrapers = load_topic(
    "text_and_encoding",
    [
        "text_normalization_and_unicode",
        "text_escaping_and_cleaning",
        "text_lists_base64_and_mapping",
    ],
)

# 1. Verify DSL
dsl_res = {
    **scrapers["text_norm_unicode"].from_string(html),
    **scrapers["text_escaping_cleaning"].from_string(html),
    **scrapers["text_lists_b64_map"].from_string(html),
}
assert dsl_res["case_and_ws"]["squished"] == "Café Résumé — Crème Brûlée"
assert dsl_res["case_and_ws"]["ascii_folded"] == "Cafe Resume — Creme Brulee"
assert dsl_res["case_and_ws"]["slug"] == "cafe_resume_creme_br"
assert len(dsl_res["case_and_ws"]["fnv1a64"]) == 16
assert len(dsl_res["case_and_ws"]["sha256"]) == 64
assert dsl_res["escapes"]["full_unescape"] == "Line1\nLine2 + OK"
assert dsl_res["escapes"]["no_url_decode"] == "Line1\nLine2 %2B%20OK"
assert dsl_res["html_ent"]["decoded"] == 'Tom & Jerry <3 "xsel2" ✓'
assert dsl_res["comments_and_replace"]["no_comments"] == "hello world"
assert dsl_res["comments_and_replace"]["replaced_map"] == "greetings earth"
assert dsl_res["tags"]["stripped_tags"] == "Alpha Beta"
assert dsl_res["lists"]["cleaned_joined"] == "[a] | [b] | [c]"
assert dsl_res["base64"]["decoded_std"] == "Hello World!"
assert dsl_res["base64"]["roundtrip_std"] == "SGVsbG8gV29ybGQh"
assert dsl_res["mapping"]["mapped"] == ["available", "delayed", "other"]

# 2. Verify Python ScraperPy fluent parity for every text/encoding method
py_res = (
    sp.ScraperPy()
    .select("h1.heading", "text")
    .text().trim().set("trim")
    .text().squish().set("squish")
    .text().normalize_whitespace().set("norm")
    .text().squish().lower().set("lower")
    .text().squish().upper().set("upper")
    .text().squish().substring(0, 5).set("sub")
    .text().squish().unicode_normalize("NFKD").strip_accents().slugify("-", 12).set("slug")
    .text().squish().hash("sha256").set("sha")
    .select("div.escaped", "esc")
    .text().unescape(url_decode=True).set("u")
    .select("div.entities", "ent")
    .text().unescape_html().set("h")
    .select("div.code-text", "code")
    .text().strip_comments(True, True).squish().replace("hello", "hey", first_only=True).set("r1")
    .text().strip_comments().squish().replace({"hello": "hi", "world": "there"}).set("r2")
    .select("div.html-snippet", "snip")
    .html().strip_tags().squish().set("clean_html")
    .select("div.csv-line", "csv")
    .text().split(",", keep_empty_parts=True).drop_empty(also_trim=True).prepend("<").append(">").join(",").set("joined")
    .select("div.b64-std", "b64")
    .text().trim().base64_decode(urlsafe=False).base64_encode(urlsafe=True).set("url_b64")
    .select("ul.statuses", "map")
    .find_all("li", sp.Value.Text).map_values({"IN_STOCK": "yes"}, default_val="no").set("statuses")
    .from_string(html)
)
assert py_res["text"]["slug"] == "cafe-resume"
assert py_res["esc"]["u"] == "Line1\nLine2 + OK"
assert py_res["ent"]["h"] == 'Tom & Jerry <3 "xsel2" ✓'
assert py_res["code"]["r1"] == "hey world"
assert py_res["code"]["r2"] == "hi there"
assert py_res["snip"]["clean_html"] == "Alpha Beta"
assert py_res["csv"]["joined"] == "<a>,<b>,<c>"
assert py_res["map"]["statuses"] == ["yes", "no", "no"]

print("ex_03_text_and_encoding: ALL ASSERTIONS PASSED")
