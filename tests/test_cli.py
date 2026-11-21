import json
import os
from pathlib import Path
import subprocess
import pytest


REPO_ROOT = Path(os.environ.get("XSEL_SOURCE_DIR", Path(__file__).resolve().parent.parent))
DEFAULT_BIN = REPO_ROOT / "build" / "xsel2"
XSEL_BIN = Path(os.environ.get("XSEL_BIN", DEFAULT_BIN))
EXAMPLES_DIR = REPO_ROOT / "examples"


def run_cli(args, stdin_text=None):
    return subprocess.run(
        [str(XSEL_BIN), *args],
        input=stdin_text,
        text=True,
        capture_output=True,
        encoding="utf-8",
    )


# ── Help & Usage ─────────────────────────────────────────────────────────────

@pytest.mark.parametrize("flag", ["-h", "--help"])
def test_help_flags(flag):
    res = run_cli([flag])
    assert res.returncode == 0
    assert "xsel2" in res.stdout
    assert "Usage:" in res.stdout
    assert "--batch" in res.stdout


def test_help_short_circuits_other_args():
    res = run_cli(["--bogus", "-h"])
    assert res.returncode == 0
    assert "Usage:" in res.stdout


def test_no_args_prints_usage_and_fails():
    res = run_cli([])
    assert res.returncode == 1
    assert "Usage:" in res.stdout


# ── Single-input Selector & DSL ──────────────────────────────────────────────

def test_selector_compact_and_pretty():
    html = "<h1 class='title'>  Hello   World </h1>"
    compact = run_cli(["-s", "h1.title", "--html", html, "-c"])
    assert compact.returncode == 0
    lines = compact.stdout.strip().splitlines()
    assert len(lines) == 1
    assert json.loads(lines[0]) == {"matches": {"text": "Hello World"}}

    pretty = run_cli(["--selector", "h1.title", "--html", html])
    assert pretty.returncode == 0
    assert len(pretty.stdout.strip().splitlines()) > 1
    assert json.loads(pretty.stdout) == {"matches": {"text": "Hello World"}}


def test_selector_empty_or_no_match():
    res = run_cli(["-s", "div.missing", "--html", "<p>hi</p>", "-c"])
    assert res.returncode == 0
    assert json.loads(res.stdout) == {}

    empty = run_cli(["-s", "p", "--html", "", "-c"])
    assert empty.returncode == 0
    assert json.loads(empty.stdout) == {}


def test_html_from_file_stdin_and_explicit_dash():
    sample_html = EXAMPLES_DIR / "sample.html"
    dsl_file = EXAMPLES_DIR / "product.dsl"

    from_file = run_cli(["-d", str(dsl_file), "-n", "summary", "--html", str(sample_html), "-c"])
    from_implicit_stdin = run_cli(["-d", str(dsl_file), "--name", "summary", "-c"], stdin_text=sample_html.read_text(encoding="utf-8"))
    from_dash_stdin = run_cli(["-d", str(dsl_file), "-n", "summary", "--html", "-", "-c"], stdin_text=sample_html.read_text(encoding="utf-8"))

    assert from_file.returncode == 0
    assert from_implicit_stdin.returncode == 0
    assert from_dash_stdin.returncode == 0

    expected = json.loads(from_file.stdout)
    assert expected["title"] == "Mechanical Keyboard Pro"
    assert json.loads(from_implicit_stdin.stdout) == expected
    assert json.loads(from_dash_stdin.stdout) == expected


def test_utf8_unicode_html():
    html = "<html><body><h1>Café Résümé 🚀 日本語</h1></body></html>"
    res = run_cli(["-s", "h1", "--html", html, "-c"])
    assert res.returncode == 0
    assert json.loads(res.stdout) == {"matches": {"text": "Café Résümé 🚀 日本語"}}


def test_dsl_multi_scraper_all_and_named():
    meta_dsl = EXAMPLES_DIR / "metadata.dsl"
    meta_html = EXAMPLES_DIR / "metadata_sample.html"

    all_res = run_cli(["-d", str(meta_dsl), "--html", str(meta_html), "-c"])
    assert all_res.returncode == 0
    all_data = json.loads(all_res.stdout)
    assert "metadata" in all_data
    assert "metadata_summary" in all_data

    named_res = run_cli(["-d", str(meta_dsl), "-n", "metadata_summary", "--html", str(meta_html), "-c"])
    assert named_res.returncode == 0
    assert json.loads(named_res.stdout) == all_data["metadata_summary"]


@pytest.mark.parametrize(
    "scraper_name,check",
    [
        ("urls_dates_unicode", lambda d: "link" in d and d["text_norm"]["slug"] == "cafe-resume-mechanical-gear"),
        ("algorithms_and_matching", lambda d: "regex_and_ac" in d and "fuzzy" in d),
        ("types_tables_and_dict", lambda d: "products" in d and "page_meta" in d),
        ("serialization_and_binary", lambda d: "binary_roundtrip" in d and "csv" in d),
        ("json_transforms_and_patch", lambda d: isinstance(d, list) and "xsel2 Showcase" in d),
    ],
)
def test_all_steps_showcase_individual_scrapers(scraper_name, check):
    dsl_file = EXAMPLES_DIR / "all_steps_showcase.dsl"
    html_file = EXAMPLES_DIR / "all_steps_showcase.html"
    res = run_cli(["-d", str(dsl_file), "-n", scraper_name, "--html", str(html_file), "-c"])
    assert res.returncode == 0
    data = json.loads(res.stdout)
    assert check(data)


# ── Batch Mode & Concurrency ─────────────────────────────────────────────────

def test_batch_ndjson_default_and_explicit(tmp_path):
    f1 = tmp_path / "a.html"
    f2 = tmp_path / "b.html"
    f1.write_text("<h1>Alpha</h1>", encoding="utf-8")
    f2.write_text("<h1>Beta</h1>", encoding="utf-8")

    for extra in ([], ["--ndjson"]):
        res = run_cli(["-s", "h1", "--batch", str(f1), str(f2), *extra])
        assert res.returncode == 0
        lines = [json.loads(line) for line in res.stdout.strip().splitlines()]
        assert len(lines) == 2
        assert lines[0] == {"file": str(f1), "result": {"matches": {"text": "Alpha"}}}
        assert lines[1] == {"file": str(f2), "result": {"matches": {"text": "Beta"}}}


def test_batch_json_array_and_object(tmp_path):
    f1 = tmp_path / "one.html"
    f2 = tmp_path / "two.html"
    f1.write_text("<h1>One</h1>", encoding="utf-8")
    f2.write_text("<h1>Two</h1>", encoding="utf-8")

    arr_res = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--json-array", "-c"])
    assert arr_res.returncode == 0
    arr = json.loads(arr_res.stdout)
    assert arr == [
        {"file": str(f1), "result": {"matches": {"text": "One"}}},
        {"file": str(f2), "result": {"matches": {"text": "Two"}}},
    ]

    obj_res = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--json-object", "-c"])
    assert obj_res.returncode == 0
    obj = json.loads(obj_res.stdout)
    assert obj == {
        str(f1): {"matches": {"text": "One"}},
        str(f2): {"matches": {"text": "Two"}},
    }


def test_batch_out_dir_creates_nested_dirs(tmp_path):
    f1 = tmp_path / "first.html"
    f2 = tmp_path / "second.html"
    f1.write_text("<h1>First</h1>", encoding="utf-8")
    f2.write_text("<h1>Second</h1>", encoding="utf-8")
    out_dir = tmp_path / "nested" / "outputs"

    res = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--out-dir", str(out_dir), "-c"])
    assert res.returncode == 0
    assert (out_dir / "first.html.json").is_file()
    assert (out_dir / "second.html.json").is_file()
    assert json.loads((out_dir / "first.html.json").read_text(encoding="utf-8")) == {"matches": {"text": "First"}}
    assert json.loads((out_dir / "second.html.json").read_text(encoding="utf-8")) == {"matches": {"text": "Second"}}


def test_batch_multithreaded_deterministic_order(tmp_path):
    files = []
    for i in range(12):
        p = tmp_path / f"doc_{i:02d}.html"
        p.write_text(f"<h1>Item {i}</h1>", encoding="utf-8")
        files.append(str(p))

    res = run_cli(["-s", "h1", "--batch", *files, "--threads", "4", "--json-array", "-c"])
    assert res.returncode == 0
    arr = json.loads(res.stdout)
    assert len(arr) == 12
    for i, item in enumerate(arr):
        assert item["file"] == files[i]
        assert item["result"] == {"matches": {"text": f"Item {i}"}}


# ── Batch Partial Failures ───────────────────────────────────────────────────

def test_batch_partial_failure_all_formats(tmp_path):
    good = tmp_path / "good.html"
    good.write_text("<h1>OK</h1>", encoding="utf-8")
    missing = tmp_path / "missing.html"

    # ndjson
    nd = run_cli(["-s", "h1", "--batch", str(good), str(missing), "--ndjson"])
    assert nd.returncode == 1
    nd_lines = [json.loads(line) for line in nd.stdout.strip().splitlines()]
    assert nd_lines[0] == {"file": str(good), "result": {"matches": {"text": "OK"}}}
    assert nd_lines[1]["file"] == str(missing)
    assert "Cannot open file" in nd_lines[1]["error"]

    # json-array
    arr_res = run_cli(["-s", "h1", "--batch", str(good), str(missing), "--json-array", "-c"])
    assert arr_res.returncode == 1
    arr = json.loads(arr_res.stdout)
    assert arr[0] == {"file": str(good), "result": {"matches": {"text": "OK"}}}
    assert "Cannot open file" in arr[1]["error"]

    # json-object
    obj_res = run_cli(["-s", "h1", "--batch", str(good), str(missing), "--json-object", "-c"])
    assert obj_res.returncode == 1
    obj = json.loads(obj_res.stdout)
    assert obj[str(good)] == {"matches": {"text": "OK"}}
    assert "Cannot open file" in obj[str(missing)]["error"]

    # out-dir
    out_dir = tmp_path / "out"
    od_res = run_cli(["-s", "h1", "--batch", str(good), str(missing), "--out-dir", str(out_dir), "-c"])
    assert od_res.returncode == 1
    assert (out_dir / "good.html.json").is_file()
    assert not (out_dir / "missing.html.json").exists()
    assert f"Error [{missing}]:" in od_res.stderr


# ── Negative & Diagnostic Error Cases ────────────────────────────────────────

@pytest.mark.parametrize(
    "args,expected_err",
    [
        (["-d"], "Missing value for flag: -d"),
        (["--dsl"], "Missing value for flag: --dsl"),
        (["-n"], "Missing value for flag: -n"),
        (["-s"], "Missing value for flag: -s"),
        (["--html"], "Missing value for flag: --html"),
        (["-s", "h1", "--batch", "a.html", "--out-dir"], "Missing value for flag: --out-dir"),
        (["-s", "h1", "--batch", "a.html", "-j"], "Missing value for flag: -j"),
        (["-s", "h1", "--html", "<p>hi</p>", "--bogus"], "Unknown flag: --bogus"),
        (["--html", "<p>hi</p>"], "--dsl (-d) or --selector (-s) is required."),
        (["-d", str(EXAMPLES_DIR / "product.dsl"), "-s", "h1", "--html", "<p>hi</p>"], "--dsl and --selector are mutually exclusive."),
        (["-s", "h1", "--html", "<p>hi</p>", "--batch", str(EXAMPLES_DIR / "sample.html")], "--html and --batch are mutually exclusive."),
        (["-s", "h1", "--batch"], "--batch requires at least one input file."),
        (["-s", "h1", "--json-array"], "Batch output/thread flags require --batch."),
        (["-s", "h1", "-j", "2"], "Batch output/thread flags require --batch."),
        (["-s", "h1", "--batch", str(EXAMPLES_DIR / "sample.html"), "--ndjson", "--json-array"], "Conflicting batch output format flag: --json-array"),
        (["-s", "h1", "--batch", str(EXAMPLES_DIR / "sample.html"), "--out-dir", "out", "--json-object"], "Conflicting batch output format flag: --json-object"),
        (["-s", "h1", "--batch", str(EXAMPLES_DIR / "sample.html"), "-j", "-1"], "Invalid non-negative integer for -j: -1"),
        (["-s", "h1", "--batch", str(EXAMPLES_DIR / "sample.html"), "--threads", "abc"], "Invalid non-negative integer for --threads: abc"),
        (["-s", "h1", "--batch", str(EXAMPLES_DIR / "sample.html"), "-j", "1.5"], "Invalid non-negative integer for -j: 1.5"),
        (["-d", str(EXAMPLES_DIR / "product.dsl"), "-n", "nonexistent", "--html", "<p>hi</p>"], "Scraper 'nonexistent' not found."),
    ],
)
def test_invalid_cli_args_stderr(args, expected_err):
    res = run_cli(args)
    assert res.returncode == 1
    assert expected_err in res.stderr


def test_empty_dsl_file_errors(tmp_path):
    empty_dsl = tmp_path / "empty.dsl"
    empty_dsl.write_text("// only comments\n", encoding="utf-8")
    res = run_cli(["-d", str(empty_dsl), "--html", "<p>hi</p>"])
    assert res.returncode == 1
    assert "No scrapers found in DSL file." in res.stderr


def test_refactored_scrapers_dsl_runs_via_cli():
    dsl = REPO_ROOT / "scrapers.dsl"
    html = "<html><body><h1 class='title'>Title</h1><div class='videoblock' data-id='42'></div></body></html>"
    res = run_cli(["-d", str(dsl), "--html", html, "-c"])
    assert res.returncode == 0
    parsed = json.loads(res.stdout)
    assert parsed["shared_page_data"]["config_inline_title"] == "Title"
    assert parsed["video_block"]["data_id"] == "42"
