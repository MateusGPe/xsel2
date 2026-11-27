import json
from pathlib import Path
import subprocess
import sys

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
SYNC_SCRIPT = REPO_ROOT / "scripts" / "sync_examples_to_docsite.py"


def _run_sync(args: list[str], extra_env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    import os

    env = os.environ.copy()
    build_dir = str(REPO_ROOT / "build")
    existing_pp = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = os.pathsep.join([p for p in [build_dir, str(REPO_ROOT), existing_pp] if p])
    if extra_env:
        env.update(extra_env)
    return subprocess.run(
        [sys.executable, str(SYNC_SCRIPT), *args],
        cwd=str(REPO_ROOT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def _make_workspace(tmp_path: Path) -> tuple[Path, Path]:
    ex_dir = tmp_path / "examples"
    ex_dir.mkdir()
    out_json = tmp_path / "docs-site" / "src" / "data" / "synced-examples.json"
    out_json.parent.mkdir(parents=True)
    return ex_dir, out_json


def test_sync_inserts_and_updates_valid_pair(tmp_path: Path) -> None:
    ex_dir, out_json = _make_workspace(tmp_path)
    (ex_dir / "demo.html").write_text("<html><body><h1>  Hello  </h1></body></html>", encoding="utf-8")
    (ex_dir / "demo.dsl").write_text(
        '// @playground id=demo label="Demo Example" scraper=main\n'
        'main = Scraper() | $("h1") { text | squish -> "title" }\n',
        encoding="utf-8",
    )

    res = _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json)])
    assert res.returncode == 0, res.stderr

    data = json.loads(out_json.read_text(encoding="utf-8"))
    assert "demo" in data["presets"]
    preset = data["presets"]["demo"]
    assert preset["label"] == "Demo Example"
    assert preset["scraper"] == "main"
    assert preset["cmd"] == "xsel2 -d demo.dsl -n main --html demo.html"
    assert "<h1>  Hello  </h1>" in preset["html"]


def test_sync_rejects_invalid_dsl(tmp_path: Path) -> None:
    ex_dir, out_json = _make_workspace(tmp_path)
    (ex_dir / "broken.html").write_text("<p>ok</p>", encoding="utf-8")
    (ex_dir / "broken.dsl").write_text('bad = Scraper() | $("p") { nonexistent_step() }\n', encoding="utf-8")

    res = _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json)])
    assert res.returncode != 0
    assert not out_json.exists()


def test_sync_dry_run_does_not_write(tmp_path: Path) -> None:
    ex_dir, out_json = _make_workspace(tmp_path)
    (ex_dir / "demo.html").write_text("<h1>Hi</h1>", encoding="utf-8")
    (ex_dir / "demo.dsl").write_text('s = Scraper() | $("h1") { text -> "t" }\n', encoding="utf-8")

    res = _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json), "--dry-run"])
    assert res.returncode == 0, res.stderr
    assert not out_json.exists()


def test_sync_keeps_missing_by_default_and_prunes_with_flag(tmp_path: Path) -> None:
    ex_dir, out_json = _make_workspace(tmp_path)
    (ex_dir / "keep.html").write_text("<h1>Keep</h1>", encoding="utf-8")
    (ex_dir / "keep.dsl").write_text('s = Scraper() | $("h1") { text -> "t" }\n', encoding="utf-8")
    (ex_dir / "temp.html").write_text("<h1>Temp</h1>", encoding="utf-8")
    (ex_dir / "temp.dsl").write_text('s = Scraper() | $("h1") { text -> "t" }\n', encoding="utf-8")

    assert _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json)]).returncode == 0

    # Delete temp example from examples/
    (ex_dir / "temp.html").unlink()
    (ex_dir / "temp.dsl").unlink()

    # Default run must NOT remove temp from synced-examples.json
    assert _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json)]).returncode == 0
    data_default = json.loads(out_json.read_text(encoding="utf-8"))
    assert "keep" in data_default["presets"]
    assert "temp" in data_default["presets"]

    # --prune removes temp, keeps keep
    assert _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json), "--prune"]).returncode == 0
    data_pruned = json.loads(out_json.read_text(encoding="utf-8"))
    assert "keep" in data_pruned["presets"]
    assert "temp" not in data_pruned["presets"]


def test_sync_remove_flag_and_protects_exclusive_presets(tmp_path: Path) -> None:
    ex_dir, out_json = _make_workspace(tmp_path)
    (ex_dir / "a.html").write_text("<h1>A</h1>", encoding="utf-8")
    (ex_dir / "a.dsl").write_text('s = Scraper() | $("h1") { text -> "t" }\n', encoding="utf-8")
    assert _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json)]).returncode == 0

    # Refuses to remove exclusive docs-site preset IDs
    res_excl = _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json), "--remove", "card"])
    assert res_excl.returncode != 0

    # Removes specific synced preset ID
    res_rm = _run_sync(["--examples-dir", str(ex_dir), "--output", str(out_json), "--remove", "a"])
    assert res_rm.returncode == 0, res_rm.stderr
    data = json.loads(out_json.read_text(encoding="utf-8"))
    assert "a" not in data["presets"]
