import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
EXAMPLES_DIR = REPO_ROOT / "examples"
STUBS_PATH = REPO_ROOT / "scraperpy-stubs" / "_scraperpy.pyi"
DSL_DOC_PATH = REPO_ROOT / "docs" / "dsl-and-steps.md"

EXPECTED_MODULES = [
    "dsl_syntax_and_composition.py",
    "dom_and_selection.py",
    "text_and_encoding.py",
    "urls_and_web.py",
    "regex_ac_and_fuzzy.py",
    "slicing_ordering_and_types.py",
    "js_digger_json_csv_queries.py",
    "dict_transforms_and_merging.py",
    "cli_all_modes.py",
]

SPLIT_DSL_FILES = [
    "dsl_syntax_and_aliases.dsl",
    "dsl_include.dsl",
    "dsl_control_flow.dsl",
    "dsl_implicit_root.dsl",
    "dom_selection_and_values.dsl",
    "xpath_tables_and_meta.dsl",
    "text_normalization_and_unicode.dsl",
    "text_escaping_and_cleaning.dsl",
    "text_lists_base64_and_mapping.dsl",
    "urls_and_query_params.dsl",
    "srcset_and_css_urls.dsl",
    "regex_extraction_and_filtering.dsl",
    "aho_corasick_and_fuzzy.dsl",
    "slicing_and_ordering.dsl",
    "types_and_numbers.dsl",
    "dates_and_durations.dsl",
    "js_json_and_csv.dsl",
    "jsonpath_projection.dsl",
    "jmespath_projection.dsl",
    "dict_transforms.dsl",
    "dict_flatten_and_pick.dsl",
]

# DSL `include(base)` requires declaring the base scraper in the same file.
MULTI_SCRAPER_DSL_EXCEPTIONS = frozenset({"dsl_include.dsl"})


def _example_env() -> dict[str, str]:
    env = os.environ.copy()
    build_dir = str(REPO_ROOT / "build")
    existing_pp = env.get("PYTHONPATH", "")
    parts = [p for p in [build_dir, str(REPO_ROOT), existing_pp] if p]
    env["PYTHONPATH"] = os.pathsep.join(parts)
    return env


@pytest.mark.parametrize("script_name", EXPECTED_MODULES)
def test_exhaustive_example_runs(script_name: str) -> None:
    script_path = EXAMPLES_DIR / script_name
    assert script_path.exists(), f"Missing exhaustive example script: {script_path}"
    res = subprocess.run(
        [sys.executable, str(script_path)],
        cwd=str(REPO_ROOT),
        env=_example_env(),
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert res.returncode == 0, (
        f"{script_name} failed (rc={res.returncode}):\n"
        f"STDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}"
    )


def test_unified_runner_succeeds() -> None:
    runner = EXAMPLES_DIR / "run_all_examples.py"
    assert runner.exists(), f"Missing unified runner: {runner}"
    res = subprocess.run(
        [sys.executable, str(runner)],
        cwd=str(REPO_ROOT),
        env=_example_env(),
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert res.returncode == 0, f"run_all_examples.py failed:\n{res.stdout}\n{res.stderr}"


def test_python_api_complete_coverage() -> None:
    stubs_text = STUBS_PATH.read_text(encoding="utf-8")
    # Extract all def <name>(...) except dunders
    methods = {
        m.group(1)
        for m in re.finditer(r"^\s*def\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(", stubs_text, re.M)
        if not m.group(1).startswith("__")
    }
    # Extract enum members (Value.* and InsertMode.*)
    enum_members = {
        m.group(1)
        for m in re.finditer(r"^\s{4}([A-Z][A-Za-z0-9_]*)\s*=\s*\.\.\.", stubs_text, re.M)
    }

    py_corpus = "\n".join(
        (EXAMPLES_DIR / name).read_text(encoding="utf-8")
        for name in EXPECTED_MODULES
        if (EXAMPLES_DIR / name).exists()
    )

    missing_methods = sorted(name for name in methods if not re.search(rf"\b{re.escape(name)}\b", py_corpus))
    missing_enums = sorted(name for name in enum_members if not re.search(rf"\b{re.escape(name)}\b", py_corpus))

    assert not missing_methods, f"Uncovered Python methods/functions in topic .py files: {missing_methods}"
    assert not missing_enums, f"Uncovered Python enum members in topic .py files: {missing_enums}"


def test_dsl_steps_and_aliases_complete_coverage() -> None:
    doc_text = DSL_DOC_PATH.read_text(encoding="utf-8")
    # Every step or alias documented as `| `name` |` in Section 2 & Section 6
    step_names = {
        m.group(1)
        for m in re.finditer(r"^\|\s*`([a-z0-9_]+)`\s*\|", doc_text, re.M)
    }
    # Add block/control-flow keywords documented outside the table rows
    step_names.update({"when_exists", "unless_exists", "save", "restore", "pipe", "use"})

    dsl_corpus = "\n".join(
        (EXAMPLES_DIR / name).read_text(encoding="utf-8")
        for name in SPLIT_DSL_FILES
        if (EXAMPLES_DIR / name).exists()
    )
    missing_steps = sorted(s for s in step_names if not re.search(rf"\b{re.escape(s)}\b", dsl_corpus))
    assert not missing_steps, f"Uncovered DSL steps/aliases/constructs in split .dsl files: {missing_steps}"


@pytest.mark.parametrize("dsl_name", SPLIT_DSL_FILES)
def test_split_dsl_files_have_single_scraper(dsl_name: str) -> None:
    if str(REPO_ROOT / "build") not in sys.path:
        sys.path.insert(0, str(REPO_ROOT / "build"))
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    import scraperpy

    dsl_path = EXAMPLES_DIR / dsl_name
    assert dsl_path.exists(), f"Missing split DSL snippet: {dsl_path}"
    scrapers = scraperpy.load_scrapers(str(dsl_path))
    if dsl_name in MULTI_SCRAPER_DSL_EXCEPTIONS:
        assert len(scrapers) == 2, f"{dsl_name} should declare base + including scraper, got {list(scrapers.keys())}"
    else:
        assert len(scrapers) == 1, f"{dsl_name} must define exactly 1 scraper, got {list(scrapers.keys())}"


def _playground_html_override(dsl_path: Path) -> Path | None:
    dsl_text = dsl_path.read_text(encoding="utf-8")
    match = re.search(r"^\s*(?://|#)\s*@playground\s+(.+)$", dsl_text, re.M)
    if not match:
        return None
    for token in shlex.split(match.group(1)):
        if token.startswith("html="):
            cand = EXAMPLES_DIR / token.split("=", 1)[1].strip()
            if cand.is_file():
                return cand
    return None


def _find_html_for_dsl(dsl_path: Path) -> Path:
    override = _playground_html_override(dsl_path)
    if override is not None:
        return override
    for cand in (
        dsl_path.with_suffix(".html"),
        EXAMPLES_DIR / f"{dsl_path.stem}_sample.html",
        EXAMPLES_DIR / "sample.html",
    ):
        if cand.is_file():
            return cand
    raise FileNotFoundError(f"No HTML fixture found for {dsl_path.name}")


@pytest.mark.parametrize("dsl_path", sorted(EXAMPLES_DIR.glob("*.dsl")), ids=lambda p: p.name)
def test_all_dsl_examples_produce_non_empty_output(dsl_path: Path) -> None:
    if str(REPO_ROOT / "build") not in sys.path:
        sys.path.insert(0, str(REPO_ROOT / "build"))
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    import scraperpy

    html_text = _find_html_for_dsl(dsl_path).read_text(encoding="utf-8")
    scrapers = scraperpy.load_scrapers(str(dsl_path))
    assert scrapers, f"No scrapers loaded from {dsl_path.name}"
    for name, scraper in scrapers.items():
        res = scraper.from_string(html_text)
        assert res, f"{dsl_path.name}::{name} returned empty output: {res!r}"


def test_docsite_presets_valid_and_non_empty() -> None:
    if str(REPO_ROOT / "build") not in sys.path:
        sys.path.insert(0, str(REPO_ROOT / "build"))
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    import scraperpy

    synced_path = REPO_ROOT / "docs-site" / "src" / "data" / "synced-examples.json"
    synced = json.loads(synced_path.read_text(encoding="utf-8")).get("presets", {})
    assert synced, "No synced presets found in synced-examples.json"
    for pid, preset in synced.items():
        scrapers = scraperpy.parse_dsl_string(preset["dsl"])
        assert scrapers, f"Synced preset {pid} parsed 0 scrapers"
        for sname, scraper in scrapers.items():
            res = scraper.from_string(preset["html"])
            assert res, f"Synced preset {pid}::{sname} returned empty output"

    playground_path = REPO_ROOT / "docs-site" / "src" / "components" / "XselPlayground.astro"
    pg_text = playground_path.read_text(encoding="utf-8")
    block = pg_text[pg_text.index("const PRESETS = {") : pg_text.index("let modulePromise = null;")]
    for pid in ("card", "js_digger", "table", "aho", "product"):
        idx = block.index(f"    {pid}: {{")
        h_start = block.index("html: `", idx) + 7
        h_end = block.index("`,\n      dsl:", h_start)
        d_start = block.index("dsl: `", h_end) + 6
        d_end = block.index("`,\n    },", d_start)
        html = block[h_start:h_end].replace("<\\/", "</")
        dsl = block[d_start:d_end].replace("\\\\", "\\").replace("\\`", "`")
        scrapers = scraperpy.parse_dsl_string(dsl)
        assert scrapers, f"Exclusive preset {pid} parsed 0 scrapers"
        for sname, scraper in scrapers.items():
            res = scraper.from_string(html)
            assert res, f"Exclusive preset {pid}::{sname} returned empty output"

