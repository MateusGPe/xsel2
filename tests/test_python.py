from pathlib import Path
import subprocess
import sys
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


def test_json_query_and_schema_helpers():
    doc = {"user": {"name": "Ada", "scores": [10, 20, 30]}}
    assert sp.jmespath_eval(doc, "user.scores[1]") == 20
    assert sp.jsonpath_query(doc, "$.user.scores[0:2]") == [10, 20]


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
        (sp.jsonpath_query, ({"a": 1}, "$[invalid")),
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
    assert {"catalog_full", "catalog_summary", "product_skus"} <= set(scrapers.keys())

    full = scrapers["catalog_full"].from_string(html)
    assert full["storefront"]["promo"] == "Featured: Save 20% on Audio & Keyboards!"
    assert full["hydration"]["apiBase"] == "https://cdn.example.com/catalog/v2"
    assert full["hydration"]["fulfillment"] == {"tier": "prime", "warehouse": "US-EAST"}
    assert full["hydration"]["deprecatedFlag"] is None
    assert "apiSecret" not in full["hydration"]["internal"]
    assert "wholesaleCost" not in full["hydration"]["internal"]
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
    s.select("p.raw").text().replace({"World": "Python"}).hash("fnv1a64").set("hash")

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
    if hasattr(sp.ScraperPy, "xpath3"):
        s.select("#box").xpath3("string-join(.//span/text(), ',')").set("joined_text")
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
    if hasattr(sp.ScraperPy, "xpath3"):
        assert out["joined_text"] == "ok"
    assert out["tbl"] == [{"item": "Pen", "qty": "5"}]
    assert out["ts"] == 1795091400
    assert out["ascii"] == "Resume"
    assert out["best"] == ["Laptop", "Laptoop"]
    assert out["scores"][0] == 1.0


@pytest.mark.skipif(not hasattr(sp.ScraperPy, "xpath3"), reason="Built without XSEL_USE_XPATH3")
def test_xpath3_python(capfd):
    html = "<ul><li data-id='1'>A</li><li data-id='2'>B</li></ul>"
    s = sp.ScraperPy()
    s.select("ul").xpath3("('a', 'b') => string-join('-')").set("joined")
    s.select("ul").xpath3(".//li", attr="data-id").set("ids")
    s.select("ul").xpath3("[1, [2, 3]]").set("arr")
    out = s.from_string(html)
    assert out["joined"] == "a-b"
    assert out["ids"] == ["1", "2"]
    assert out["arr"] == ["1", "2", "3"]

    with pytest.raises(Exception):
        sp.ScraperPy().select("ul").xpath3("///invalid[[[")

    bad_out = sp.ScraperPy().select("ul").xpath3("map { 'k': 'v' }").set("m").from_string(html)
    assert bad_out == {}
    assert "FOTY0014" in capfd.readouterr().err


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


def test_python_find_replace_and_regex_defaults():
    html = """
    <div class="box">
      <a class="lnk" href="/a?b=c">  One   Two  </a>
      <span class="txt">x-x-x</span>
      <span class="case">FOO foo</span>
    </div>
    """
    s = (
        sp.ScraperPy()
        .select(".box")
        .find("a.lnk")
        .set("link_text")
        .find("a.lnk", attr="href")
        .set("href")
        .find(".txt")
        .replace("x", "y")
        .set("rep_all")
        .find(".txt")
        .replace("x", "y", first_only=True)
        .set("rep_first")
        .find(".case")
        .regex_replace("foo", "bar")
        .set("rx_default")
        .find(".case")
        .regex_replace("foo", "bar", flags="icase")
        .set("rx_icase")
    )
    out = s.from_string(html)
    assert out["link_text"] == "One Two"
    assert out["href"] == "/a?b=c"
    assert out["rep_all"] == "y-y-y"
    assert out["rep_first"] == "y-x-x"
    assert out["rx_default"] == "FOO bar"
    assert out["rx_icase"] == "bar bar"


def test_python_json_parse_lenient():
    s = sp.ScraperPy()
    s.select("script").text().json_parse("data", lenient=True)
    out = s.from_string("<script>{unquoted: 'a' + 'b', n: 42, extra: undefined,}</script>")
    assert out["data"] == {"unquoted": "ab", "n": 42, "extra": None}

    # Merging into root dict when objname is empty
    s_root = sp.ScraperPy()
    s_root.select("script").text().json_parse(lenient=True)
    out_root = s_root.from_string("<script>{k: 'v', list: [1, 2,]}</script>")
    assert out_root == {"k": "v", "list": [1, 2]}


def test_python_module_parse_js_and_json_parse():
    # parse_js uses digger::parse_object directly
    js = "{unquoted: 'a' + 'b', n: 42, extra: undefined, list: [1, 2,]}"
    assert sp.parse_js(js) == {"unquoted": "ab", "n": 42, "extra": None, "list": [1, 2]}
    assert sp.parse_js("") is None

    # simplify=False disables '+' concatenation and other simplifications
    raw = sp.parse_js("{s: 'a' + 'b'}", simplify=False)
    assert raw == {"s": "a"}

    # json_parse strict by default (raises RuntimeError on non-JSON)
    assert sp.json_parse('{"a": 1, "b": [true, null]}') == {"a": 1, "b": [True, None]}
    with pytest.raises(RuntimeError):
        sp.json_parse(js)

    # json_parse with JSON options
    assert sp.json_parse('/* c */ {"a": 1,}', allow_comments=True, allow_trailing_comma=True) == {"a": 1}

    # json_parse with lenient=True delegates to digger::parse_object
    assert sp.json_parse(js, lenient=True) == {
        "unquoted": "ab",
        "n": 42,
        "extra": None,
        "list": [1, 2],
    }
    assert sp.json_parse("{s: 'a' + 'b'}", lenient=True, simplify=False) == {"s": "a"}

    # Empty or whitespace/punctuation-only input returns None in lenient/parse_js mode without throwing
    assert sp.parse_js("   ") is None
    assert sp.json_parse("", lenient=True) is None
    assert sp.json_parse(";;;", lenient=True) is None


def test_python_phase1_to_3_and_single_parse_document():
    html = """
    <html>
      <head>
        <script type="application/ld+json">{"@type": "VideoObject", "name": "Demo"}</script>
      </head>
      <body>
        <h1>  Fast   Scraper  </h1>
        <img class="pic" data-src="https://cdn.test/img.jpg" srcset="a.jpg 200w, b.jpg 800w" />
        <span class="views">1.2K</span>
        <span class="dur">01:30</span>
        <span class="dt">yesterday</span>
        <a class="lnk" href="https://example.com/v?id=1&utm_source=x">Go</a>
      </body>
    </html>
    """
    doc = sp.Document.from_string(html)
    s1 = (
        sp.ScraperPy()
        .select("html")
        .fields({
            "title": "h1",
            "cover": {"sel": "img.pic", "attr": ["src", "data-src"]},
            "has_pic": {"sel": "img.pic", "type": "exists"},
        })
        .find(".views").parse_number(compact=True, key="views")
        .find(".dur").parse_duration(key="dur_s")
        .find(".dt").parse_date(output="date", now="2026-11-20T12:00:00Z").set("posted")
        .find("img.pic", sp.Value.Attribute, "srcset").parse_srcset("largest").set("best_img")
        .find("a.lnk", sp.Value.Attribute, "href").strip_query(drop_params=["utm_*"]).set("clean_url")
        .extract_meta("meta", index_json_ld=True)
    )
    out1 = s1.from_document(doc)
    assert out1["title"] == "Fast Scraper"
    assert out1["cover"] == "https://cdn.test/img.jpg"
    assert out1["has_pic"] is True
    assert out1["views"] == 1200
    assert out1["dur_s"] == 90
    assert out1["posted"] == "2026-11-19"
    assert out1["best_img"] == "b.jpg"
    assert out1["clean_url"] == "https://example.com/v?id=1"
    assert out1["meta"]["json_ld_by_type"]["VideoObject"]["name"] == "Demo"

    s2 = sp.ScraperPy().select("h1").text().squish().set("heading")
    many = sp.scrape_many({"s1": s1, "s2": s2}, html)
    assert many["s1"] == out1
    assert many["s2"] == {"heading": "Fast Scraper"}

    # Ensure all_steps_showcase.dsl loads cleanly in Python
    repo_root = Path(__file__).resolve().parent.parent
    showcase_scrapers = sp.load_scrapers(str(repo_root / "examples" / "all_steps_showcase.dsl"))
    assert {"dom_and_text", "urls_dates_unicode", "algorithms_and_matching",
            "types_tables_and_dict", "serialization", "json_transforms",
            "debug_trace"} <= set(showcase_scrapers.keys())


def test_python_outer_html_rename_and_merge_keys():
    html = """
    <div class="card">
      <h1>Item</h1>
      <span class="t1">alpha</span>
      <span class="t2">beta</span>
    </div>
    """
    s = (
        sp.ScraperPy()
        .select("div.card")
        .outer_html()
        .set("markup")
        .find_all(".t1", sp.Value.Text)
        .set("tags_a")
        .find_all(".t2", sp.Value.Text)
        .set("tags_b")
        .rename_keys({"markup": "card_html"})
        .merge_keys(["tags_a", "tags_b"], "all_tags")
    )
    out = s.from_string(html)
    assert out["card_html"].startswith('<div class="card">')
    assert out["card_html"].endswith("</div>")
    assert out["all_tags"] == ["alpha", "beta"]
    assert "tags_a" not in out and "tags_b" not in out

    # Even-length list form for rename_keys
    s_list = (
        sp.ScraperPy()
        .select("div.card h1")
        .text()
        .set("old_name")
        .rename_keys(["old_name", "new_name"])
    )
    assert s_list.from_string(html) == {"new_name": "Item"}

    with pytest.raises(RuntimeError):
        sp.ScraperPy().rename_keys(["unpaired"])
