"""Example 04: URL resolution, parts, query_param(s), strip_query, filter_urls, parse_srcset, css_url."""

import scraperpy as sp
from _helpers import load_pair

html, scrapers = load_pair("ex_04_urls_and_web")
BASE = "https://shop.example.com:443/catalog/index.html"

# 1. Verify DSL
dsl_res = scrapers["urls_and_web"].from_string(html)
parts = dsl_res["url_parts"]
assert parts["scheme"] == "https"
assert parts["host"] == "shop.example.com"
assert parts["port"] == "443"
assert parts["path"] == "/products/item-1"
assert parts["fragment"] == "specs"
assert parts["q_decoded"] == "hello world"
assert parts["q_missing"] == "fallback"
assert dsl_res["bracket_params"]["referrer"] == "home"
assert dsl_res["bracket_params"]["color"] == "blue"
assert dsl_res["bracket_params"]["opt"] == "n/a"
assert parts["stripped_all"] == "https://shop.example.com:443/products/item-1#specs"
assert "utm_source" not in parts["dropped_utm"]
assert "https://evil.org/phish" not in dsl_res["filtered_links"]["safe_example_urls"]
assert dsl_res["srcset_demo"]["largest"] == "https://shop.example.com:443/img/large.jpg"
assert dsl_res["srcset_demo"]["smallest"] == "https://shop.example.com:443/img/small.jpg"
assert len(dsl_res["srcset_demo"]["all_urls"]) == 3
assert dsl_res["srcset_dpr"]["best_dpr"] == "/img/2x.png"
assert dsl_res["css_urls"]["from_style_tag"] == "https://shop.example.com:443/assets/bg-main.png"
assert dsl_res["css_urls"]["from_inline_style"] == "https://shop.example.com:443/assets/hero.webp"

# 2. Verify Python ScraperPy parity
py_res = (
    sp.ScraperPy()
    .select("a.rel", "u")
    .attr("href").resolve_url(BASE).url_part("path").set("path")
    .attr("href").resolve_url(BASE).query_param("ref", "").set("ref")
    .attr("href").resolve_url(BASE).query_params(["ref", "q"], default_val="").set("qp_list")
    .attr("href").resolve_url(BASE).strip_query(keep_params=["ref"]).set("kept")
    .select("nav.links", "f")
    .find_all("a", sp.Value.Attribute, "href")
    .resolve_url(BASE)
    .filter_urls(allowed_domains=["example.com"], blocked_domains=["evil.org"], allowed_schemes=["https"], same_origin_of="")
    .set("allowed")
    .select("img.hero", "img")
    .attr("srcset").parse_srcset("largest").set("best")
    .select("style", "css")
    .text().css_url().set("bg")
    .from_string(html)
)
assert py_res["u"]["path"] == "/products/item-1"
assert py_res["u"]["ref"] == "home"
assert py_res["img"]["best"] == "/img/large.jpg"
assert py_res["css"]["bg"] == "/assets/bg-main.png"

print("ex_04_urls_and_web: ALL ASSERTIONS PASSED")
