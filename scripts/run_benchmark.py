#!/usr/bin/env python3
"""Benchmark runner comparing xsel2 (-j 0 and -j 1) against Python + BeautifulSoup + lxml."""

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def run_and_measure(cmd: list[str], cwd: Path | None = None) -> tuple[float, float, int]:
    """Run command with stdout -> /dev/null and return (wall_ms, peak_rss_mb, returncode)."""
    t0 = time.perf_counter()
    with open(os.devnull, "wb") as devnull:
        proc = subprocess.Popen(cmd, cwd=cwd, stdout=devnull, stderr=subprocess.PIPE)
        _, status, rusage = os.wait4(proc.pid, 0)
    wall_ms = (time.perf_counter() - t0) * 1000.0
    rc = os.waitstatus_to_exitcode(status)
    if rc != 0:
        err = proc.stderr.read().decode("utf-8", errors="replace") if proc.stderr else ""
        raise RuntimeError(f"Command failed ({rc}): {' '.join(cmd)}\n{err}")
    if proc.stderr:
        proc.stderr.close()
    peak_rss_mb = rusage.ru_maxrss / 1024.0
    return wall_ms, peak_rss_mb, rc


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark xsel2 vs BeautifulSoup + lxml")
    parser.add_argument(
        "--html-dir",
        type=Path,
        default=Path("/home/mateusgp/projects/htmls"),
        help="Directory containing HTML files",
    )
    parser.add_argument(
        "--xsel2-bin",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "build" / "xsel2",
        help="Path to xsel2 binary",
    )
    parser.add_argument(
        "--dsl",
        type=Path,
        default=Path(__file__).resolve().parent / "benchmark.dsl",
        help="Path to DSL file",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Optional limit on number of files (0 = all)",
    )
    args = parser.parse_args()

    html_dir = args.html_dir.resolve()
    files = sorted(p.name for p in html_dir.iterdir() if p.is_file() and p.suffix == ".html")
    if args.limit > 0 and args.limit < len(files):
        step = len(files) / args.limit
        files = [files[int(i * step)] for i in range(args.limit)]

    total_bytes = sum((html_dir / name).stat().st_size for name in files)
    print(f"Dataset: {len(files):,} HTML files, {total_bytes / (1024**3):.2f} GiB ({total_bytes / 1e9:.2f} GB)")

    xsel_bin = str(args.xsel2_bin.resolve())
    dsl_path = str(args.dsl.resolve())
    bs4_script = str((Path(__file__).resolve().parent / "benchmark_bs4.py").resolve())

    # 1. xsel2 -j 0 (multi-threaded)
    t_j0, rss_j0, _ = run_and_measure(
        [xsel_bin, "-d", dsl_path, "-n", "real_scrape", "--batch", *files, "-j", "0", "-c"],
        cwd=html_dir,
    )
    print(f"xsel2 (-j 0): {t_j0/1000:.2f} s ({t_j0:.1f} ms), Peak RSS: {rss_j0:.1f} MB", flush=True)

    # 2. xsel2 -j 1 (single-threaded)
    t_j1, rss_j1, _ = run_and_measure(
        [xsel_bin, "-d", dsl_path, "-n", "real_scrape", "--batch", *files, "-j", "1", "-c"],
        cwd=html_dir,
    )
    print(f"xsel2 (-j 1): {t_j1/1000:.2f} s ({t_j1:.1f} ms), Peak RSS: {rss_j1:.1f} MB", flush=True)

    # 3. Python + BeautifulSoup + lxml (single-process)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tf:
        for name in files:
            tf.write(str(html_dir / name) + "\n")
        list_path = tf.name

    try:
        t_py, rss_py, _ = run_and_measure(
            [sys.executable, bs4_script, "--file-list", list_path],
            cwd=html_dir,
        )
    finally:
        os.unlink(list_path)

    print(f"Python + bs4 + lxml: {t_py/1000:.2f} s ({t_py:.1f} ms), Peak RSS: {rss_py:.1f} MB", flush=True)
    print(f"Speedup (-j 0 vs bs4): {t_py / t_j0:.1f}x")
    print(f"Speedup (-j 1 vs bs4): {t_py / t_j1:.1f}x")
    print(f"Memory ratio (bs4 / -j 0): {rss_py / rss_j0:.1f}x")


if __name__ == "__main__":
    main()
