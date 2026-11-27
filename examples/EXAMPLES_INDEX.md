# Exhaustive Examples Index & Coverage Matrix

Run all exhaustive examples at once:

```bash
PYTHONPATH=build:. python3 examples/run_all_examples.py
```

Sync `.dsl` + `.html` examples into `docs-site` Playground presets:

```bash
PYTHONPATH=build:. python3 scripts/sync_examples_to_docsite.py
# Optional flags:
#   --dry-run   Validate without writing
#   --prune     Remove synced presets whose source files were deleted from examples/
#   --remove ID Remove specific synced preset ID(s) (never touches exclusive docs-site presets)
```

---

## Module Overview

| Module | Files | Coverage |
| --- | --- | --- |
| **01. DSL Syntax & Composition** | [`ex_01_dsl_syntax_and_composition.dsl`](ex_01_dsl_syntax_and_composition.dsl), [`.py`](ex_01_dsl_syntax_and_composition.py), [`.html`](ex_01_dsl_syntax_and_composition.html) | Comments (`#`, `//`, `/* */`), `let`, `${var}`, `\${var}`, multiline strings, `=`, `:`, `:=`, implicit root, `.`/`|`/`\|>`, `.(...)`, `$`, `@`, `??`, `[0]`, `[-1]`, `->`/`>>`/`as`, all 17 aliases, `pipe`/`use`, `include`, `tee`/`branch`, `save`/`restore`, `each`, nested `$`, `when_exists`, `unless_exists`, `when_match`, `skip_if_exists`, `skip_if_empty`, `abort_if_exists`, `finalize`, `@key`/`@value`, `parse_dsl_string`, `load_scrapers` |
| **02. DOM, XPath, Tables & Metadata** | [`ex_02_dom_and_selection.dsl`](ex_02_dom_and_selection.dsl), [`.py`](ex_02_dom_and_selection.py), [`.html`](ex_02_dom_and_selection.html) | `select`, `text(own_text)`, `attr`, `find`, `find_all`, all 6 `Value` modes (`Text`, `OwnText`, `Attribute`, `ElementExists`, `AttributeExists`, `Adjacent`), `extract_fields`/`fields`, `html`, `outer_html`, `tag_name`, `classes`, `attrs(data_only)`, `xpath`, `xpath3`, `table`, `extract_meta` |
| **03. Text, Encoding & Unicode** | [`ex_03_text_and_encoding.dsl`](ex_03_text_and_encoding.dsl), [`.py`](ex_03_text_and_encoding.py), [`.html`](ex_03_text_and_encoding.html) | `trim`, `squish`, `normalize_whitespace`, `lower`, `upper`, `unescape`, `unescape_html`, `replace` (scalar & mapping), `split`, `join`, `prepend`, `append`, `drop_empty`, `substring`, `strip_comments`, `strip_tags`, `base64_encode`, `base64_decode`, `hash` (`fnv1a64`, `sha256`), `slugify`, `map_values`, `unicode_normalize` (`NFC`/`NFD`/`NFKC`/`NFKD`), `strip_accents` |
| **04. URLs, Srcset & CSS URL** | [`ex_04_urls_and_web.dsl`](ex_04_urls_and_web.dsl), [`.py`](ex_04_urls_and_web.py), [`.html`](ex_04_urls_and_web.html) | `resolve_url`, `url_part` (`scheme`/`host`/`port`/`path`/`query`/`fragment`), `query_param`, `query_params`, `strip_query`, `filter_urls`, `parse_srcset`, `css_url` |
| **05. Regex, Aho-Corasick & Fuzzy** | [`ex_05_regex_ac_and_fuzzy.dsl`](ex_05_regex_ac_and_fuzzy.dsl), [`.py`](ex_05_regex_ac_and_fuzzy.py), [`.html`](ex_05_regex_ac_and_fuzzy.html) | `regex_replace`, `regex_filter`, `regex_extract`, `regex_find_all`, `regex_groups` (`i`/`s`/`m` flags), `filter`, `extract_many`, `fuzzy_filter`, `similarity`, `AhoCorasick` (`add_pattern`, `build`, `clear`, `pattern_count`, `contains_any`, `has_all`, `search`) |
| **06. Slicing, Types & Dates** | [`ex_06_slicing_ordering_and_types.dsl`](ex_06_slicing_ordering_and_types.dsl), [`.py`](ex_06_slicing_ordering_and_types.py), [`.html`](ex_06_slicing_ordering_and_types.html) | `first`, `last`, `nth`, `slice`, `unique`, `sort`, `reverse`, `default`, `set(type=...)`, `append_to`, `count`, `parse_number`, `parse_duration`, `to_int`, `to_float`, `to_bool`, `parse_date` |
| **07. JS Digger, JSON, CSV & Queries** | [`ex_07_js_digger_json_csv_queries.dsl`](ex_07_js_digger_json_csv_queries.dsl), [`.py`](ex_07_js_digger_json_csv_queries.py), [`.html`](ex_07_js_digger_json_csv_queries.html) | `js_block`, `js_blocks`, `parse_js`, `json_parse`, `json_dump`, `csv_parse`, `csv_dump`, `jmespath`, `jsonpath`, `json_schema`, Python `jmespath_eval`, `jsonpath_query`, `json_schema_validate`, `parse_js`, `json_parse` |
| **08. Dict Transforms, Merging & Docs** | [`ex_08_dict_transforms_and_merging.dsl`](ex_08_dict_transforms_and_merging.dsl), [`.py`](ex_08_dict_transforms_and_merging.py), [`.html`](ex_08_dict_transforms_and_merging.html) | `flatten`, `pick_keys`, `omit_keys`, `rename_keys`, `merge_keys`, `zip_keys`, `coalesce_keys`, `cast_keys`, `sort_array`, `deduplicate`, `debug`, `InsertMode` (`Default`, `Overwrite`, `AppendToList`, `UniqueSuffix`, `Merge`), `InsertOptions`, `Document`, `from_document`, `from_file`, `scrape_many` |
| **09. CLI All Modes** | [`ex_09_cli_all_modes.py`](ex_09_cli_all_modes.py) | `-h`/`--help`, `-s`/`--selector`, `-d`/`--dsl`, `-n`/`--name`, `--html` (file, string, `-`, stdin), `-c`/`--compact`, `--batch`, `--ndjson`, `--json-array`, `--json-object`, `--out-dir`, `-j`/`--threads` |

---

## Automated Coverage Verification

`tests/test_examples.py` automatically verifies that:
- Every `ex_01`–`ex_09` script exits with code `0`.
- **100%** of methods, enums, and functions in `scraperpy-stubs/_scraperpy.pyi` are exercised in `ex_*.py`.
- **100%** of DSL steps, aliases, and control-flow keywords in `docs/dsl-and-steps.md` are exercised in `ex_*.dsl`.
