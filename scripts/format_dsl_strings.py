#!/usr/bin/env python3
"""Format long string literals in xsel2 DSL scripts and presets into multiline strings.

Supports formatting complex structured queries (e.g. JMESPath expressions)
with clean, readable block indentation across multiple lines.
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Tuple


def format_jmespath_query(expr: str, base_indent: str, step_indent: str = "  ") -> str:
    """Format a JMESPath expression string into a clean multiline structure."""
    text = expr.strip()
    if not text.startswith("{") or not text.endswith("}"):
        return expr

    # Tokenize while respecting quotes and backticks
    tokens = []
    i = 0
    n = len(text)
    cur = []

    level = 0
    while i < n:
        c = text[i]
        if c in ('"', "'", '`'):
            quote = c
            cur.append(c)
            i += 1
            while i < n:
                ch = text[i]
                cur.append(ch)
                if ch == '\\' and i + 1 < n:
                    i += 1
                    cur.append(text[i])
                elif ch == quote:
                    break
                i += 1
        elif c in ('{', '[', '('):
            cur.append(c)
            level += 1
        elif c in ('}', ']', ')'):
            cur.append(c)
            level -= 1
        elif c == ',' and level == 1:
            tokens.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1

    if cur:
        rem = "".join(cur).strip()
        if rem:
            tokens.append(rem)

    if not tokens or len(tokens) == 1:
        return expr

    # Clean tokens: remove leading '{' from first token and trailing '}' from last
    first = tokens[0]
    if first.startswith("{"):
        tokens[0] = first[1:].strip()

    last = tokens[-1]
    if last.endswith("}"):
        tokens[-1] = last[:-1].strip()

    inner_indent = base_indent + step_indent
    lines = ["{"]

    for idx, tok in enumerate(tokens):
        comma = "," if idx < len(tokens) - 1 else ""

        # Check if tok contains an inner object projection like foo[?...].{a: 1, b: 2}
        sub_match = re.search(r'^(.*?\.?\{)(.*?)(\})$', tok)
        if sub_match and ',' in sub_match.group(2):
            sub_prefix = sub_match.group(1).strip()
            sub_body = sub_match.group(2).strip()
            sub_items = [item.strip() for item in sub_body.split(',') if item.strip()]

            lines.append(f"{inner_indent}{sub_prefix}")
            sub_indent = inner_indent + step_indent
            for s_idx, s_item in enumerate(sub_items):
                s_comma = "," if s_idx < len(sub_items) - 1 else ""
                lines.append(f"{sub_indent}{s_item}{s_comma}")
            lines.append(f"{inner_indent}}}{comma}")
        else:
            lines.append(f"{inner_indent}{tok}{comma}")

    lines.append(f"{base_indent}}}")
    return "\n".join(lines)


def format_dsl_content(content: str, max_line_length: int = 80) -> Tuple[str, bool]:
    """Format long string arguments in DSL content. Returns (new_content, changed)."""
    # Regex to detect jmespath("...") or jmespath('...')
    # Handles both standalone DSL and Astro embedded presets (where backticks might be escaped: \`50\`)
    pattern = re.compile(
        r'([ \t]*)(?:[.|]\s*)?(jmespath)\s*\(\s*(["\'])(.*?)\3\s*\)',
        re.DOTALL
    )

    changed = False

    def repl(m: re.Match) -> str:
        nonlocal changed
        indent = m.group(1)
        full_match = m.group(0)

        # Only format if the matched line or string is sufficiently long or already contains multiple fields
        expr = m.group(4)
        if len(full_match) < max_line_length and '\n' not in expr:
            return full_match

        # Format expression
        formatted_expr = format_jmespath_query(expr, base_indent=indent + "  ", step_indent="  ")
        if formatted_expr == expr:
            return full_match

        changed = True
        quote = m.group(3)
        # Determine prefix operator (. or |)
        orig_before_func = full_match[:m.start(2) - m.start(0)]
        return f'{orig_before_func}jmespath({quote}{formatted_expr}\n{indent}{quote})'

    new_content = pattern.sub(repl, content)
    return new_content, (new_content != content)


def process_file(file_path: Path, write: bool = False, max_line_length: int = 80) -> bool:
    """Process a single file. Returns True if formatting was needed/performed."""
    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Error reading {file_path}: {e}", file=sys.stderr)
        return False

    new_content, changed = format_dsl_content(content, max_line_length)

    if changed:
        if write:
            file_path.write_text(new_content, encoding="utf-8")
            print(f"Formatted: {file_path}")
        else:
            print(f"Needs formatting: {file_path}")
        return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Format long DSL string literals into clean multiline strings."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to format. Defaults to examples/ and docs-site/src/components/XselPlayground.astro",
    )
    parser.add_argument(
        "-w", "--write",
        action="store_true",
        help="Write formatted changes in-place.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Don't write files, exit with code 1 if files need formatting.",
    )
    parser.add_argument(
        "--max-length",
        type=int,
        default=80,
        help="Line length threshold to trigger multiline formatting (default: 80).",
    )

    args = parser.parse_args()

    target_paths = args.paths
    if not target_paths:
        target_paths = ["examples", "docs-site/src/components/XselPlayground.astro"]

    files_to_check: List[Path] = []
    for p_str in target_paths:
        p = Path(p_str)
        if p.is_file():
            files_to_check.append(p)
        elif p.is_dir():
            for ext in ("*.dsl", "*.astro", "*.md"):
                files_to_check.extend(p.rglob(ext))

    unformatted_count = 0
    for f in sorted(set(files_to_check)):
        if process_file(f, write=args.write, max_line_length=args.max_length):
            unformatted_count += 1

    if args.check and unformatted_count > 0:
        print(f"\n{unformatted_count} file(s) need formatting. Run with --write to format.", file=sys.stderr)
        return 1

    if unformatted_count == 0:
        print("All target files are cleanly formatted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
