#!/usr/bin/env python3
"""Equivalent BeautifulSoup + lxml + regex scraper for benchmarking against xsel2."""

import json
import re
import sys
from pathlib import Path
from bs4 import BeautifulSoup

RE_ASCII_WS = re.compile(r"[ \t\r\n\f\v]+")
RE_ITEM_ID = re.compile(r"item_id=([a-zA-Z0-9]+)")
RE_DIGITS = re.compile(r"([0-9]+)")
RE_JS_MARKER = re.compile(r"var\s+PAGE_STATE\s*=\s*")


def squish(text: str | None) -> str | None:
    if text is None:
        return None
    return RE_ASCII_WS.sub(" ", text).strip()


def extract_js_object(script_text: str) -> dict | None:
    m = RE_JS_MARKER.search(script_text)
    if not m:
        return None
    start = script_text.find("{", m.end())
    if start == -1:
        return None
    depth = 0
    in_str = False
    escape = False
    quote_char = ""
    for i in range(start, len(script_text)):
        ch = script_text[i]
        if in_str:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote_char:
                in_str = False
        else:
            if ch in ('"', "'"):
                in_str = True
                quote_char = ch
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    raw = script_text[start : i + 1]
                    try:
                        return json.loads(raw)
                    except json.JSONDecodeError:
                        return None
    return None


def scrape_file(path: Path) -> dict:
    html = path.read_bytes()
    soup = BeautifulSoup(html, "lxml")

    title_el = soup.select_one("title")
    page_title = squish(title_el.get_text()) if title_el else None

    og_el = soup.select_one('meta[property="og:title"]')
    og_title = squish(og_el.get("content")) if og_el and og_el.get("content") is not None else None

    canon_el = soup.select_one('link[rel="canonical"]')
    canon_href = canon_el.get("href") if canon_el else None
    canonical_url = canon_href.strip() if canon_href is not None else None

    item_id = None
    if canonical_url:
        m = RE_ITEM_ID.search(canonical_url)
        if m:
            item_id = m.group(1)

    h1_el = soup.select_one("h1")
    heading = squish(h1_el.get_text()) if h1_el else None

    rating_el = soup.select_one(".rating-percent .percent, .media-rating .percent")
    rating_pct = None
    if rating_el:
        m = RE_DIGITS.search(rating_el.get_text())
        if m:
            rating_pct = m.group(1)

    seen_tags = set()
    tags = []
    for el in soup.select(".tag-list a, .category-list a"):
        val = squish(el.get_text())
        if val and val not in seen_tags:
            seen_tags.add(val)
            tags.append(val)

    page_state = None
    for sc in soup.find_all("script"):
        txt = sc.string if sc.string is not None else sc.get_text()
        if "var PAGE_STATE =" in txt:
            page_state = extract_js_object(txt)
            if page_state is not None:
                break

    return {
        "file": str(path),
        "result": {
            "page_title": page_title,
            "og_title": og_title,
            "canonical_url": canonical_url,
            "item_id": item_id,
            "heading": heading,
            "rating_pct": rating_pct,
            "tags": tags if tags else None,
            "item_num_id": page_state.get("item_id") if page_state else None,
            "item_key": page_state.get("key") if page_state else None,
            "segment": page_state.get("segment") if page_state else None,
        },
    }


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: benchmark_bs4.py [--file-list <list.txt> | <file1> ...]", file=sys.stderr)
        sys.exit(1)

    if sys.argv[1] == "--file-list":
        list_path = Path(sys.argv[2])
        files = [Path(line) for line in list_path.read_text().splitlines() if line.strip()]
    else:
        files = [Path(p) for p in sys.argv[1:]]

    out = sys.stdout.write
    for f in files:
        res = scrape_file(f)
        out(json.dumps(res, ensure_ascii=False, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    main()
