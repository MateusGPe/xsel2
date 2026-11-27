#!/usr/bin/env python3
"""Unified runner for all exhaustive examples (ex_01_*.py .. ex_09_*.py)."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys

EXAMPLES_DIR = Path(__file__).resolve().parent
REPO_ROOT = EXAMPLES_DIR.parent


def main() -> int:
    env = os.environ.copy()
    build_dir = str(REPO_ROOT / "build")
    existing_pp = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = os.pathsep.join([p for p in [build_dir, str(REPO_ROOT), existing_pp] if p])

    scripts = sorted(EXAMPLES_DIR.glob("ex_*.py"))
    if not scripts:
        print("No ex_*.py scripts found.", file=sys.stderr)
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
