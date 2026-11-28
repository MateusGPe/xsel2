#!/usr/bin/env python3
"""Unified runner for all exhaustive topic examples."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys

EXAMPLES_DIR = Path(__file__).resolve().parent
REPO_ROOT = EXAMPLES_DIR.parent

TOPIC_SCRIPTS = [
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


def main() -> int:
    env = os.environ.copy()
    build_dir = str(REPO_ROOT / "build")
    existing_pp = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = os.pathsep.join([p for p in [build_dir, str(REPO_ROOT), existing_pp] if p])

    scripts = [EXAMPLES_DIR / name for name in TOPIC_SCRIPTS]
    missing = [s.name for s in scripts if not s.exists()]
    if missing:
        print(f"Missing topic scripts: {missing}", file=sys.stderr)
        return 1

    failures = 0
    for script in scripts:
        res = subprocess.run(
            [sys.executable, str(script)],
            cwd=str(REPO_ROOT),
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if res.returncode == 0:
            print(f"[PASS] {script.name}")
        else:
            failures += 1
            print(f"[FAIL] {script.name} (rc={res.returncode})", file=sys.stderr)
            if res.stdout:
                print(res.stdout, file=sys.stderr)
            if res.stderr:
                print(res.stderr, file=sys.stderr)

    print(f"\nSummary: {len(scripts) - failures}/{len(scripts)} examples passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
