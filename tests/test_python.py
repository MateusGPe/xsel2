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
