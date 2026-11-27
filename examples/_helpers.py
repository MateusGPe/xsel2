"""Shared DRY helpers for the exhaustive xsel2 / scraperpy examples."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import scraperpy as sp

EXAMPLES_DIR = Path(__file__).resolve().parent
REPO_ROOT = EXAMPLES_DIR.parent


def find_xsel_bin() -> Path:
    """Locate the compiled xsel2 CLI binary."""
    env_bin = os.environ.get("XSEL_BIN")
    if env_bin:
        return Path(env_bin)
    for candidate in (
        REPO_ROOT / "build" / "xsel2.exe",
        REPO_ROOT / "build" / "xsel2",
        REPO_ROOT / "build-release" / "xsel2.exe",
        REPO_ROOT / "build-release" / "xsel2",
    ):
        if candidate.exists():
            return candidate
    return REPO_ROOT / "build" / "xsel2"


def load_pair(stem: str) -> tuple[str, dict[str, sp.ScraperPy]]:
    """Load `<stem>.html` and parse `<stem>.dsl` into a dict of scrapers."""
    html_path = EXAMPLES_DIR / f"{stem}.html"
    dsl_path = EXAMPLES_DIR / f"{stem}.dsl"
    html = html_path.read_text(encoding="utf-8")
    scrapers = sp.load_scrapers(str(dsl_path))
    return html, scrapers


def run_all_dsl(scrapers: dict[str, sp.ScraperPy], html: str) -> dict[str, Any]:
    """Execute every scraper in `scrapers` against `html`."""
    return sp.scrape_many(scrapers, html)
