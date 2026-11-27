"""Example 09: Exhaustive CLI (xsel2) verification across all flags and batch modes."""

import json
import subprocess
import tempfile
from pathlib import Path

from _helpers import EXAMPLES_DIR, find_xsel_bin

XSEL_BIN = find_xsel_bin()
assert XSEL_BIN.exists(), f"xsel2 CLI binary not found at {XSEL_BIN}"


def run_cli(args: list[str], stdin_text: str | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(XSEL_BIN), *args],
        input=stdin_text,
        text=True,
        capture_output=True,
        encoding="utf-8",
    )


# 1. -h and --help
for flag in ("-h", "--help"):
    r = run_cli([flag])
    assert r.returncode == 0
    assert "Usage:" in r.stdout

# 2. -s / --selector with inline --html, file --html, '-' stdin, implicit stdin, and -c / --compact
inline_html = "<html><body><h1 class='t'>  Hello   CLI  </h1></body></html>"
r_inline = run_cli(["-s", "h1.t", "--html", inline_html, "-c"])
assert r_inline.returncode == 0
assert json.loads(r_inline.stdout) == {"matches": {"text": "Hello CLI"}}

r_pretty = run_cli(["--selector", "h1.t", "--html", inline_html])
assert r_pretty.returncode == 0
assert len(r_pretty.stdout.strip().splitlines()) > 1

r_dash = run_cli(["-s", "h1.t", "--html", "-", "-c"], stdin_text=inline_html)
assert r_dash.returncode == 0
assert json.loads(r_dash.stdout) == {"matches": {"text": "Hello CLI"}}

r_implicit_stdin = run_cli(["-s", "h1.t", "--compact"], stdin_text=inline_html)
assert r_implicit_stdin.returncode == 0
assert json.loads(r_implicit_stdin.stdout) == {"matches": {"text": "Hello CLI"}}

# 3. -d / --dsl with and without -n / --name
dsl_path = str(EXAMPLES_DIR / "ex_02_dom_and_selection.dsl")
html_path = str(EXAMPLES_DIR / "ex_02_dom_and_selection.html")

r_named = run_cli(["-d", dsl_path, "-n", "dom_and_values", "--html", html_path, "-c"])
assert r_named.returncode == 0
assert json.loads(r_named.stdout)["box"]["tag"] == "section"

r_all_scrapers = run_cli(["--dsl", dsl_path, "--html", html_path, "-c"])
assert r_all_scrapers.returncode == 0
all_out = json.loads(r_all_scrapers.stdout)
assert "dom_and_values" in all_out and "xpath_tables_meta" in all_out

# 4. --batch modes: --ndjson (default), --json-array, --json-object, --out-dir, -j / --threads
with tempfile.TemporaryDirectory() as td:
    tmp = Path(td)
    f1 = tmp / "a.html"
    f2 = tmp / "b.html"
    f1.write_text("<h1>First</h1>", encoding="utf-8")
    f2.write_text("<h1>Second</h1>", encoding="utf-8")

    # --ndjson with -j 2
    r_ndjson = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--ndjson", "-j", "2"])
    assert r_ndjson.returncode == 0
    nd_lines = [json.loads(line) for line in r_ndjson.stdout.strip().splitlines()]
    assert len(nd_lines) == 2

    # --json-array with -j 0 (auto threads)
    r_arr = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--json-array", "-j", "0", "-c"])
    assert r_arr.returncode == 0
    arr = json.loads(r_arr.stdout)
    assert isinstance(arr, list) and len(arr) == 2

    # --json-object with --threads 1
    r_obj = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--json-object", "--threads", "1", "-c"])
    assert r_obj.returncode == 0
    obj = json.loads(r_obj.stdout)
    assert len(obj) == 2

    # --out-dir
    out_dir = tmp / "out"
    r_outdir = run_cli(["-s", "h1", "--batch", str(f1), str(f2), "--out-dir", str(out_dir), "-j", "0", "-c"])
    assert r_outdir.returncode == 0
    assert (out_dir / "a.html.json").exists() and (out_dir / "b.html.json").exists()

print("ex_09_cli_all_modes: ALL ASSERTIONS PASSED")
