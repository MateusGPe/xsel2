# CLI Reference

`xsel2` builds two command-line executables: `xmlsel-cli` (multi-tool CLI) and `xmlsel-pipe` (lightweight Unix-pipe scraper).

## Input Resolution

For `xmlsel-cli`, arguments marked `<file|str|->` are resolved automatically:
- `-` reads from `stdin`.
- If the value is a readable file path on disk, its contents are read.
- Otherwise, the argument is treated as a literal inline string.

---

## `xmlsel-cli`

```text
xmlsel-cli <subcommand> [options]
```

Run `xmlsel-cli --help` (or `-h`) to print usage.

### 1. `scraper`

Extracts structured JSON from HTML/XML using either a `.dsl` file or a one-off CSS selector.

```bash
xmlsel-cli scraper (--dsl <file> [--name <scraper>] | --selector <css>) --html <file|str|-> [--compact]
```

| Flag | Required | Description |
| --- | --- | --- |
| `--dsl <file>` | Yes (unless `--selector`) | Path to a `.dsl` file |
| `--name <scraper>` | No | Name of a specific scraper in the `.dsl` file. If omitted and the file defines one scraper, that scraper's result is printed; if multiple scrapers are defined, all are run and keyed by name |
| `--selector <css>` | Yes (unless `--dsl`) | Quick single-selector extraction (`Select(selector, "matches").Text().Squish().Set("text")`) |
| `--html <file\|str\|->` | Yes | HTML input file, inline HTML string, or `-` for `stdin` |
| `--compact`, `-c` | No | Emit single-line compact JSON instead of pretty-printed JSON |

**Examples:**

```bash
# Quick inline HTML extraction
xmlsel-cli scraper --selector "h1" --html "<html><body><h1>  Hello  </h1></body></html>" --compact

# Run a named scraper from a .dsl file
xmlsel-cli scraper --dsl examples/product.dsl --name summary --html examples/sample.html

# Read HTML from stdin
cat examples/sample.html | xmlsel-cli scraper --dsl examples/product.dsl --name product --html -
```

### 2. `ahocorasick`

Scans text for multiple literal patterns simultaneously using the Aho-Corasick automaton and outputs a JSON array of matches (`start`, `end`, `match`, `pattern_id`).

```bash
xmlsel-cli ahocorasick --patterns <p1,p2,...> --text <file|str|->
```

| Flag | Required | Description |
| --- | --- | --- |
| `--patterns <p1,p2,...>` | Yes | Comma-separated list of literal patterns |
| `--text <file\|str\|->` | Yes | Input text file, inline string, or `-` for `stdin` |

**Example:**

```bash
xmlsel-cli ahocorasick --patterns "Mechanical,USB-C,Bluetooth" --text examples/sample.html
```

### 3. `jmespath` & `jsonpath`

Evaluates a JMESPath or JSONPath expression against a JSON document.

```bash
xmlsel-cli jmespath --expr <expr> --json <file|str|-> [--compact]
xmlsel-cli jsonpath --expr <expr> --json <file|str|-> [--compact]
```

| Flag | Required | Description |
| --- | --- | --- |
| `--expr <expr>` | Yes | JMESPath or JSONPath expression |
| `--json <file\|str\|->` | Yes | JSON file, inline JSON string, or `-` for `stdin` |
| `--compact`, `-c` | No | Emit single-line compact JSON |

**Examples:**

```bash
# Inline JSON query
xmlsel-cli jmespath --expr "items[?price > \`10\`].name" --json '{"items":[{"name":"A","price":5},{"name":"B","price":20}]}'

# Query JSON piped from another command
xmlsel-cli scraper --dsl examples/product.dsl --name product --html examples/sample.html -c \
  | xmlsel-cli jsonpath --expr "$[*].state.tags[*]" --json -
```

---

## `xmlsel-pipe`

`xmlsel-pipe` is a minimal filter designed for shell pipelines: it always reads HTML from `stdin` and writes JSON to `stdout`.

```bash
xmlsel-pipe -d <dsl_file> [-n <scraper_name>] [-c]
```

| Flag | Required | Description |
| --- | --- | --- |
| `-d`, `--dsl <file>` | Yes | Path to the `.dsl` file |
| `-n`, `--name <name>` | No | Specific scraper name to run (if omitted: runs the sole scraper, or returns an object keyed by scraper name when multiple exist) |
| `-c`, `--compact` | No | Emit single-line compact JSON |
| `-h`, `--help` | No | Print usage and exit |

**Example:**

```bash
cat examples/sample.html | xmlsel-pipe -d examples/product.dsl -n summary -c
```
