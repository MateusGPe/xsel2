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
