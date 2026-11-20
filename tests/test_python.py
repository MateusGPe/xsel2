from pathlib import Path
import pytest

import scraperpy as sp


SAMPLE_HTML = """
<html>
  <body>
    <h1 class="title">  Widget   Pro  </h1>
    <ul class="tags">
      <li>fast</li>
      <li>cheap</li>
      <li>fast</li>
    </ul>
    <script>
      window.__STATE__ = { name: "Widget", count: 3, active: true };
    </script>
  </body>
</html>
"""


def test_scraper_basic_extraction():
    scraper = (
        sp.ScraperPy()
        .select("h1.title")
        .text()
        .squish()
        .set("title")
        .select("ul.tags")
        .find_all("li", sp.Value.Text)
        .unique()
        .set("tags")
    )
    res = scraper.from_string(SAMPLE_HTML)
    assert res["title"] == "Widget Pro"
    assert res["tags"] == ["fast", "cheap"]


def test_scraper_block_parser_and_types():
    scraper = (
        sp.ScraperPy()
        .select("script")
        .text()
        .extract_js_blocks(["window.__STATE__"])
    )
    res = scraper.from_string(SAMPLE_HTML)
    state = res["window"]["__STATE__"]
    assert state == {"name": "Widget", "count": 3, "active": True}


def test_scraper_empty_html():
    scraper = sp.ScraperPy().select("h1").text().default("N/A").set("title")
    res = scraper.from_string("")
    assert res == {}


def test_ahocorasick_full_api():
    ac = sp.AhoCorasick()
    assert ac.add_pattern("alpha") == 0
    assert ac.add_pattern("beta") == 1
    assert ac.pattern_count() == 2
    ac.build()

    assert ac.contains_any("xxalphayy")
    assert not ac.contains_any("gamma")
    assert ac.has_all("alpha and beta")
    assert not ac.has_all("alpha only")

    matches = ac.search("alpha beta alpha")
    assert [(m[0], m[1], m[2], m[3]) for m in matches] == [
        (0, 5, "alpha", 0),
        (6, 10, "beta", 1),
        (11, 16, "alpha", 0),
    ]

    with pytest.raises(RuntimeError):
        ac.add_pattern("gamma")

    ac.clear()
    assert ac.pattern_count() == 0
    assert ac.add_pattern("gamma") == 0
    ac.build()
    assert ac.contains_any("gamma")


def test_dsl_parse_and_load_file():
    dsl = """
    product = Scraper()
      .select("h1.title").text().squish() -> "title"
      .select("ul.tags").find_all("li", Text).unique() -> "tags";
    """
    scrapers = sp.parse_dsl_string(dsl)
    assert "product" in scrapers
    res = scrapers["product"].from_string(SAMPLE_HTML)
    assert res["title"] == "Widget Pro"
    assert res["tags"] == ["fast", "cheap"]

    repo_root = Path(__file__).resolve().parent.parent
    loaded = sp.load_scrapers(str(repo_root / "examples" / "product.dsl"))
    assert len(loaded) >= 1
    first_scraper = next(iter(loaded.values()))
    sample_res = first_scraper.from_file(str(repo_root / "examples" / "sample.html"))
    assert "Mechanical Keyboard Pro" in str(sample_res)


def test_metadata_dsl_exhaustive():
    repo_root = Path(__file__).resolve().parent.parent
    scrapers = sp.load_scrapers(str(repo_root / "examples" / "metadata.dsl"))
    assert "metadata" in scrapers
    assert "metadata_summary" in scrapers

    html_path = str(repo_root / "examples" / "metadata_sample.html")
    meta = scrapers["metadata"].from_file(html_path)
    summary = scrapers["metadata_summary"].from_file(html_path)

    assert meta["basic"]["lang"] == "en-US"
    assert meta["basic"]["charset"] == "utf-8"
    assert meta["basic"]["title"] == "Definitive Guide to HTML Metadata & Schemas"
    assert meta["basic"]["fediverse_creator"] == "@ada@mastodon.social"
    assert meta["http_equiv"]["x_ua_compatible"] == "IE=edge"
    assert meta["open_graph"]["title"] == "Definitive Guide to HTML Metadata"
    assert meta["open_graph"]["locale_alternate"] == ["pt_BR", "es_ES"]
    assert meta["open_graph"]["article"]["tag"] == ["HTML", "OpenGraph"]
    assert meta["open_graph"]["product"]["price_amount"] == "49.90"
    assert meta["twitter"]["card"] == "summary_large_image"
    assert meta["twitter"]["label1"] == "Reading time"
    assert meta["twitter"]["app"]["iphone"]["id"] == "123456789"
    assert meta["facebook"]["app_id"] == "9876543210"
    assert meta["instagram_and_applinks"]["al_android"]["package"] == "com.techeditorial.reader"
    assert meta["dublin_core"]["dc_creator"] == "Ada Lovelace"
    assert meta["academic_and_publishing"]["citation_author"] == ["Lovelace, Ada", "Turing, Alan"]
    assert meta["apple"]["mobile_web_app_capable"] == "yes"
    assert meta["microsoft"]["tile_color"] == "#0f172a"
    assert meta["pinterest"]["rich_pin"] == "true"
    assert meta["verification"]["google"] == "google-verify-abc"
    assert meta["links"]["canonical"] == "https://example.com/articles/html-metadata"
    assert meta["links"]["webmention"] == "https://example.com/webmention"
    assert meta["json_ld"]["@type"] == "Article"
    assert meta["embedded_json"]["next_data"]["props"]["pageProps"]["articleId"] == 42
    assert meta["embedded_json"]["initial_state"]["user"]["role"] == "editor"
    assert meta["embedded_json"]["importmap"]["imports"]["lodash"] == "https://esm.sh/lodash@4.17.21"
    assert meta["microdata"]["scopes"]["itemtype"] == "https://schema.org/TechArticle"
    assert any(p["itemprop"] == "datePublished" and p["datetime"] == "2026-11-19" for p in meta["microdata"]["props"])
    assert meta["definition_lists"]["lists"]["id"] == "article-meta"
    assert meta["definition_lists"]["terms"][0]["text"] == "Language"
    assert meta["definition_lists"]["descriptions"][0]["itemprop"] == "inLanguage"
    assert meta["definition_lists"]["descriptions"][0]["text"] == "en-US"
    assert meta["rdfa"]["types"]["typeof"] == "Organization"
    assert len(meta["all_meta"]) >= 40
    assert len(meta["all_links"]) >= 10

    assert summary["title"] == "Definitive Guide to HTML Metadata"
    assert summary["url"] == "https://example.com/articles/html-metadata"
    assert summary["image"] == "https://example.com/images/cover.jpg"
    assert summary["lang"] == "en-US"
    assert summary["json_ld"]["@type"] == "Article"


def test_json_query_and_patch_helpers():
    doc = {"user": {"name": "Ada", "scores": [10, 20, 30]}}
    assert sp.jmespath_eval(doc, "user.scores[1]") == 20
    assert sp.jsonpath_query(doc, "$.user.scores[0:2]") == [10, 20]
    assert sp.jsonpointer_get(doc, "/user/name") == "Ada"

    patched = sp.json_patch_apply(
        doc, [{"op": "replace", "path": "/user/name", "value": "Grace"}]
    )
    assert patched["user"]["name"] == "Grace"

    merged = sp.merge_patch_apply(doc, {"user": {"role": "admin"}})
    assert merged["user"]["role"] == "admin"

    flat = sp.json_flatten({"a": {"b": [1, 2]}})
    assert sp.json_unflatten(flat) == {"a": {"b": [1, 2]}}


def test_json_schema_validate():
    schema = {
        "type": "object",
        "required": ["name"],
        "properties": {"name": {"type": "string"}},
    }
    ok, errors = sp.json_schema_validate({"name": "Ada"}, schema)
    assert ok is True
    assert errors == []

    ok, errors = sp.json_schema_validate({"name": 123}, schema)
    assert ok is False
    assert len(errors) > 0


@pytest.mark.parametrize(
    "fn,args",
    [
        (sp.jmespath_eval, ({}, "invalid[[")),
        (sp.jsonpointer_get, ({"a": 1}, "/missing")),
        (sp.parse_dsl_string, ("broken = (;",)),
    ],
)
def test_error_propagation(fn, args):
    with pytest.raises(RuntimeError):
        fn(*args)


def test_marketplace_example_synergy():
    repo_root = Path(__file__).resolve().parent.parent
    html = (repo_root / "examples" / "marketplace_sample.html").read_text(encoding="utf-8")

    # Aho-Corasick pre-scan
    ac = sp.AhoCorasick()
    for pat in ["window.__CATALOG_STATE__", "window.__PROMO_CONFIG__", "application/ld+json", "text/csv"]:
        ac.add_pattern(pat)
    ac.build()
    assert ac.has_all(html)

    scrapers = sp.load_scrapers(str(repo_root / "examples" / "marketplace.dsl"))
    assert {"catalog_full", "catalog_summary", "product_skus", "flattened_storefront"} <= set(scrapers.keys())

    full = scrapers["catalog_full"].from_string(html)
    assert full["storefront"]["verified"] is True
    assert full["storefront"]["promo"] == "Featured: Save 20% on Audio & Keyboards!"
    assert full["hydration"]["apiBase"] == "https://cdn.example.com/catalog/v2"
    assert full["hydration"]["fulfillment"] == {"tier": "prime", "warehouse": "US-EAST"}
    assert full["hydration"]["deprecatedFlag"] is None
    assert full["hydration"]["internal"]["apiSecret"] == "***REDACTED***"
    assert "wholesaleCost" not in full["hydration"]["internal"]
    assert full["hydration"]["environment"] == "production"
    assert full["hydration"]["featuredSkus"] == ["APX-101", "APX-202"]
    assert full["window"]["__PROMO_CONFIG__"]["code"] == "SPRING20"
    assert full["json_ld"]["numberOfItems"] == 3
    assert len(full["shipping_rates"]) == 3
    assert "_schema_errors" not in full

    summary = scrapers["catalog_summary"].from_string(html)
    assert summary["warehouse"] == "US-EAST"
    assert [p["sku"] for p in summary["in_stock_products"]] == ["APX-101", "APX-202"]
    assert summary["shipping_carriers"] == ["ApexAir", "GroundX", "EuroPost"]

    assert scrapers["product_skus"].from_string(html) == ["APX-101", "APX-202", "APX-303"]

    flat = scrapers["flattened_storefront"].from_string(html)
    assert sp.json_unflatten(flat)["storefront"]["canonical"] == "https://marketplace.example.com/collections/audio-peripherals"

    assert sp.jsonpath_query(full, "$.products[?(@.category == 'audio')].name") == ["Studio Monitor Headphones"]
    assert sp.jmespath_eval(full, "window.__PROMO_CONFIG__.discountPct") == 20


def test_tier1_steps_python():
    html = """
    <div id="item" class="card hot" data-sku="A1">
      <span class="price">$1,050.25</span>
      <span class="qty">4</span>
      <a class="go" href="/p/9?id=9&utm_source=x">Link</a>
      <p class="raw">Hello <b>World</b></p>
    </div>
    """
    s = sp.ScraperPy()
    s.select(".price").text().parse_number(key="price")
    s.select(".qty").text().set("qty", type="int")
    s.select("a.go").attr("href").resolve_url("https://shop.test/").strip_query(["id"]).set("url")
    s.select("p.raw").html().strip_tags().set("clean_text")
    s.select("#item").attrs(data_only=True)
    s.select("p.raw").text().replace_many({"World": "Python"}).hash("fnv1a64").set("hash")

    out = s.from_string(html)
    assert out["price"] == 1050.25
    assert isinstance(out["qty"], int) and out["qty"] == 4
    assert out["url"] == "https://shop.test/p/9?id=9"
    assert out["clean_text"] == "Hello World"
    assert out["data-sku"] == "A1"
    assert len(out["hash"]) == 16


def test_tier2_steps_python():
    html = """
    <html>
      <head>
        <title>PyTest Page</title>
        <meta property="og:type" content="website">
        <link rel="canonical" href="https://example.org/">
      </head>
      <body>
        <div id="box"><span data-code="X99">ok</span></div>
        <table><tr><th>Item</th><th>Qty</th></tr><tr><td>Pen</td><td>5</td></tr></table>
        <span class="dt">2026-11-19T12:30:00Z</span>
        <span class="acc">Résumé</span>
        <ul class="opts"><li>Laptop</li><li>Laptoop</li><li>Desk</li></ul>
      </body>
    </html>
    """
    s = sp.ScraperPy()
    s.select("head").extract_meta("meta")
    s.select("#box").xpath(".//span", attr="data-code").set("code")
    s.select("table").table("tbl", header=True, normalize_headers=True)
    s.select(".dt").text().parse_date(output="unix", key="ts")
    s.select(".acc").text().unicode_normalize("NFKD").strip_accents().set("ascii")
    s.select(".opts").find_all("li", sp.Value.Text).fuzzy_filter("Laptop", threshold=0.85, sort_by_score=True).set("best")
    s.select(".opts").find_all("li", sp.Value.Text).similarity("Laptop", algo="jaro_winkler", key="scores")

    out = s.from_string(html)
    assert out["meta"]["title"] == "PyTest Page"
    assert out["meta"]["og:type"] == "website"
    assert out["meta"]["canonical"] == "https://example.org/"
    assert out["code"] == "X99"
    assert out["tbl"] == [{"item": "Pen", "qty": "5"}]
    assert out["ts"] == 1795091400
    assert out["ascii"] == "Resume"
    assert out["best"] == ["Laptop", "Laptoop"]
    assert out["scores"][0] == 1.0


def test_python_bridge_edge_cases():
    # bool vs int distinction & 64-bit integer boundaries
    payload = {
        "b_true": True,
        "b_false": False,
        "i_zero": 0,
        "i_one": 1,
        "i64_min": -(2**63),
        "i64_max": 2**63 - 1,
        "u64_max": 2**64 - 1,
    }
    rt = sp.jmespath_eval(payload, "@")
    assert rt["b_true"] is True
    assert rt["b_false"] is False
    assert type(rt["i_zero"]) is int and rt["i_zero"] == 0
    assert type(rt["i_one"]) is int and rt["i_one"] == 1
    assert rt["i64_min"] == -(2**63)
    assert rt["i64_max"] == 2**63 - 1
    assert rt["u64_max"] == 2**64 - 1

    # Oversized int (> uint64_t max) & unsupported types must raise
    with pytest.raises(Exception):
        sp.jmespath_eval({"huge": 10**30}, "@")
    with pytest.raises(RuntimeError):
        sp.jmespath_eval({"bad": {1, 2, 3}}, "@")

    # Depth limit in python_to_json (MAX_JSON_DEPTH = 512)
    deep = 1
    for _ in range(550):
        deep = [deep]
    with pytest.raises(RuntimeError):
        sp.jmespath_eval(deep, "@")


def test_python_scraper_edge_cases():
    s_empty = sp.ScraperPy()
    s_empty.select("h1").text().set("title")
    assert s_empty.from_string("") == {}

    s_missing = sp.ScraperPy()
    s_missing.select("div").find(".missing", sp.Value.Text).set("v")
    assert s_missing.from_string("<div><span>ok</span></div>") == {}

    with pytest.raises(ValueError):
        sp.ScraperPy().select("div").regex_replace("[unclosed", "x")
