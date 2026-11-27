import json
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
EXAMPLES_DIR = REPO_ROOT / "examples"
STUBS_PATH = REPO_ROOT / "scraperpy-stubs" / "_scraperpy.pyi"
DSL_DOC_PATH = REPO_ROOT / "docs" / "dsl-and-steps.md"

EXPECTED_MODULES = [
    "ex_01_dsl_syntax_and_composition.py",
    "ex_02_dom_and_selection.py",
    "ex_03_text_and_encoding.py",
    "ex_04_urls_and_web.py",
    "ex_05_regex_ac_and_fuzzy.py",
    "ex_06_slicing_ordering_and_types.py",
    "ex_07_js_digger_json_csv_queries.py",
    "ex_08_dict_transforms_and_merging.py",
    "ex_09_cli_all_modes.py",
]


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

    assert not missing_methods, f"Uncovered Python methods/functions in ex_*.py: {missing_methods}"
    assert not missing_enums, f"Uncovered Python enum members in ex_*.py: {missing_enums}"


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
        p.read_text(encoding="utf-8")
        for p in sorted(EXAMPLES_DIR.glob("ex_*.dsl"))
    )
    missing_steps = sorted(s for s in step_names if not re.search(rf"\b{re.escape(s)}\b", dsl_corpus))
    assert not missing_steps, f"Uncovered DSL steps/aliases/constructs in ex_*.dsl: {missing_steps}"
