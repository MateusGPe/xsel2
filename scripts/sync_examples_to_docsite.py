#!/usr/bin/env python3
"""Validate and sync runnable .dsl + .html examples from examples/ into docs-site."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXAMPLES_DIR = REPO_ROOT / "examples"
DEFAULT_OUTPUT_JSON = REPO_ROOT / "docs-site" / "src" / "data" / "synced-examples.json"

# Exclusive presets defined directly inside docs-site/src/components/XselPlayground.astro
EXCLUSIVE_PRESET_IDS = frozenset(
    {"card", "js_digger", "table", "aho", "product", "table_data", "selector"}
)

_SCRAPER_DECL_RE = re.compile(
    r"^(?!let\b|pipe\b)([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|:|:=)\s*(?:Scraper|ScraperPy|\.)",
    re.MULTILINE,
)
_PLAYGROUND_META_RE = re.compile(r"^\s*(?://|#)\s*@playground\s+(.+)$", re.MULTILINE)


def _humanize_label(stem: str) -> str:
    cleaned = re.sub(r"^ex_\d+_", "", stem)
    return " ".join(part.capitalize() for part in cleaned.split("_") if part)


def _parse_playground_meta(dsl_text: str) -> dict[str, str]:
    match = _PLAYGROUND_META_RE.search(dsl_text)
    if not match:
        return {}
    raw = match.group(1).strip()
    if raw == "skip":
        return {"skip": "true"}
    meta: dict[str, str] = {}
    for token in shlex.split(raw):
        if "=" in token:
            k, v = token.split("=", 1)
            meta[k.strip()] = v.strip()
    return meta


def _find_matching_html(examples_dir: Path, stem: str) -> Path | None:
    candidates = [
        examples_dir / f"{stem}.html",
        examples_dir / f"{stem}_sample.html",
    ]
    if stem == "product":
        candidates.append(examples_dir / "sample.html")
    for cand in candidates:
        if cand.is_file():
            return cand
    return None


def _detect_first_scraper(dsl_text: str) -> str | None:
    match = _SCRAPER_DECL_RE.search(dsl_text)
    return match.group(1) if match else None


def _validate_pair(dsl_text: str, html_text: str, scraper_name: str, dsl_path: Path, html_path: Path) -> None:
    if not html_text.strip():
        raise ValueError(f"Empty HTML fixture for {dsl_path.name}")
    if not dsl_text.strip():
        raise ValueError(f"Empty DSL file {dsl_path.name}")

    try:
        import scraperpy as sp

        scrapers = sp.parse_dsl_string(dsl_text)
        if scraper_name not in scrapers:
            raise ValueError(
                f"Scraper '{scraper_name}' not found in {dsl_path.name} "
                f"(available: {sorted(scrapers.keys())})"
            )
        scrapers[scraper_name].from_string(html_text)
        return
    except ImportError:
        pass

    for candidate in (
        REPO_ROOT / "build" / "xsel2.exe",
        REPO_ROOT / "build" / "xsel2",
    ):
        if candidate.exists():
            res = subprocess.run(
                [str(candidate), "-d", str(dsl_path), "-n", scraper_name, "--html", str(html_path), "-c"],
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            if res.returncode != 0:
                raise ValueError(f"xsel2 CLI validation failed for {dsl_path.name}: {res.stderr}")
            return

    raise RuntimeError("Neither scraperpy nor build/xsel2 is available to validate examples.")


def discover_and_validate(examples_dir: Path) -> dict[str, dict[str, Any]]:
    discovered: dict[str, dict[str, Any]] = {}
    for dsl_path in sorted(examples_dir.glob("*.dsl")):
        dsl_text = dsl_path.read_text(encoding="utf-8")
        meta = _parse_playground_meta(dsl_text)
        if meta.get("skip") == "true":
            continue

        html_path = _find_matching_html(examples_dir, dsl_path.stem)
        if html_path is None:
            continue
        html_text = html_path.read_text(encoding="utf-8")

        preset_id = meta.get("id", dsl_path.stem)
        if preset_id in EXCLUSIVE_PRESET_IDS:
            # Preserve exclusive docs-site presets without overwriting them
            continue

        scraper_name = meta.get("scraper") or _detect_first_scraper(dsl_text)
        if not scraper_name:
            raise ValueError(f"Could not detect a scraper declaration in {dsl_path.name}")

        _validate_pair(dsl_text, html_text, scraper_name, dsl_path, html_path)

        label = meta.get("label") or _humanize_label(dsl_path.stem)
        discovered[preset_id] = {
            "id": preset_id,
            "label": label,
            "cmd": f"xsel2 -d {dsl_path.name} -n {scraper_name} --html {html_path.name}",
            "html": html_text,
            "dsl": dsl_text,
            "source_dsl": dsl_path.name,
            "source_html": html_path.name,
            "scraper": scraper_name,
        }
    return discovered


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate and sync examples/*.dsl + *.html into docs-site Playground presets."
    )
    parser.add_argument("--examples-dir", type=Path, default=DEFAULT_EXAMPLES_DIR)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT_JSON)
    parser.add_argument(
        "--prune",
        action="store_true",
        help="Remove synced presets whose source files no longer exist in examples/.",
    )
    parser.add_argument(
        "--remove",
        nargs="+",
        default=[],
        metavar="ID",
        help="Remove specific synced preset ID(s) from docs-site without affecting exclusive presets.",
    )
    parser.add_argument(
        "--dry-run",
        "--validate-only",
        dest="dry_run",
        action="store_true",
        help="Validate and report planned changes without writing the output JSON.",
    )
    args = parser.parse_args(argv)

    for rid in args.remove:
        if rid in EXCLUSIVE_PRESET_IDS:
            print(
                f"Error: '{rid}' is an exclusive docs-site preset and cannot be removed by this script.",
                file=sys.stderr,
            )
            return 2

    existing_presets: dict[str, dict[str, Any]] = {}
    if args.output.exists():
        loaded = json.loads(args.output.read_text(encoding="utf-8"))
        existing_presets = dict(loaded.get("presets", {}))

    discovered = discover_and_validate(args.examples_dir)

    merged: dict[str, dict[str, Any]] = dict(existing_presets)
    merged.update(discovered)

    if args.prune:
        merged = {
            pid: pdata
            for pid, pdata in merged.items()
            if pid in discovered
        }

    for rid in args.remove:
        merged.pop(rid, None)

    ordered = {k: merged[k] for k in sorted(merged.keys())}
    payload = {"presets": ordered}

    print(
        f"Validated {len(discovered)} example pair(s); "
        f"output presets: {len(ordered)} "
        f"(prune={args.prune}, dry_run={args.dry_run})."
    )

    if not args.dry_run:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
