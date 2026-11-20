# CLI Reference

`xsel2` is a unified command-line HTML/XML scraping tool that supports single inputs, Unix `stdin` pipelines, and multi-threaded batch processing.

## Input Resolution

Arguments marked `<file|str|->` are resolved automatically:
- `-` reads from `stdin`.
- If the value is a readable file path on disk, its contents are read.
- Otherwise, the argument is treated as a literal inline string.

When `--html` is omitted (and `--batch` is not used), `xsel2` reads HTML from `stdin` automatically.

---

## Usage

```text
xsel2 (-d <file> [-n <scraper>] | -s <css>) [--html <file|str|->] [-c]
xsel2 (-d <file> [-n <scraper>] | -s <css>) --batch <file1> [<file2> ...] [--ndjson | --json-array | --json-object | --out-dir <dir>] [-j <N>] [-c]
```

Run `xsel2 --help` (or `-h`) to print usage.

### Options

| Flag | Required | Description |
| --- | --- | --- |
| `-d`, `--dsl <file>` | Yes (unless `-s`) | Path to a `.dsl` file |
| `-n`, `--name <scraper>` | No | Name of a specific scraper in the `.dsl` file. If omitted and the file defines one scraper, that scraper's result is printed; if multiple scrapers are defined, all are run and keyed by name |
| `-s`, `--selector <css>` | Yes (unless `-d`) | Quick single-selector extraction (`Select(selector, "matches").Text().Squish().Set("text")`) |
| `--html <file\|str\|->` | No | HTML input file, inline HTML string, or `-` for `stdin` (defaults to `stdin` when omitted) |
| `-c`, `--compact` | No | Emit single-line compact JSON instead of pretty-printed JSON |
| `--batch <files...>` | No | Process one or more HTML/XML files (mutually exclusive with `--html`) |
| `--ndjson` | No | Batch output: one compact JSON object `{"file":..., "result":...}` per line on `stdout` (default) |
| `--json-array` | No | Batch output: single JSON array `[{"file":..., "result":...}, ...]` on `stdout` |
| `--json-object` | No | Batch output: single JSON object keyed by file path on `stdout` |
| `--out-dir <dir>` | No | Batch output: write `<dir>/<filename>.json` for each input file |
| `-j`, `--threads <N>` | No | Worker thread count for `--batch` (default `1`; `0` = auto-detect hardware concurrency) |
| `-h`, `--help` | No | Print usage and exit |

---

## Examples

### Single Input & Pipe Mode

```bash
# Quick inline HTML extraction
xsel2 -s "h1" --html "<html><body><h1>  Hello  </h1></body></html>" -c

# Run a named scraper from a .dsl file
xsel2 -d examples/product.dsl -n summary --html examples/sample.html

# Run all scrapers in a .dsl file (when -n is omitted)
xsel2 -d examples/metadata.dsl --html examples/metadata_sample.html

# Stream HTML from stdin (pipe mode)
cat examples/sample.html | xsel2 -d examples/product.dsl -n summary -c
```

### Batch & Multi-Threaded Batch

```bash
# NDJSON output (default) using 4 worker threads
xsel2 -d examples/product.dsl -n summary --batch examples/*.html -j 4

# Single JSON array with auto-detected thread count
xsel2 -s "h1" --batch examples/sample.html examples/marketplace_sample.html --json-array -j 0 -c

# Single JSON object keyed by file path
xsel2 -d examples/product.dsl --batch examples/sample.html examples/metadata_sample.html --json-object

# Write one .json file per input into out/
xsel2 -d examples/product.dsl -n summary --batch examples/*.html --out-dir out -j 0 -c
```
