# Exhaustive Examples Index & Coverage Matrix

Run all exhaustive topic examples at once:

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

## Topic Overview

Each topic shares one `.html` fixture and one `.py` verification script, while its `.dsl` examples are split into focused, single-scraper snippets (except `dsl_include.dsl`, which declares a base scraper for `include()`).

| Topic | DSL Snippets | Shared HTML & Python | Coverage |
| --- | --- | --- | --- |
| **DSL Syntax & Composition** | [`dsl_syntax_and_aliases.dsl`](dsl_syntax_and_aliases.dsl), [`dsl_include.dsl`](dsl_include.dsl), [`dsl_control_flow.dsl`](dsl_control_flow.dsl), [`dsl_implicit_root.dsl`](dsl_implicit_root.dsl) | [`.html`](dsl_syntax_and_composition.html), [`.py`](dsl_syntax_and_composition.py) | Comments (`#`, `//`, `/* */`), `let`, `${var}`, `\${var}`, multiline strings, `=`, `:`, `:=`, implicit root, `.`/`|`/`\|>`, `.(...)`, `$`, `@`, `??`, `[0]`, `[-1]`, `->`/`>>`/`as`, all 17 aliases, `pipe`/`use`, `include`, `tee`/`branch`, `save`/`restore`, `each`, nested `$`, `when_exists`, `unless_exists`, `when_match`, `skip_if_exists`, `skip_if_empty`, `abort_if_exists`, `finalize`, `@key`/`@value`, `parse_dsl_string`, `load_scrapers` |
| **DOM, XPath, Tables & Metadata** | [`dom_selection_and_values.dsl`](dom_selection_and_values.dsl), [`xpath_tables_and_meta.dsl`](xpath_tables_and_meta.dsl) | [`.html`](dom_and_selection.html), [`.py`](dom_and_selection.py) | `select`, `text(own_text)`, `attr`, `find`, `find_all`, all 6 `Value` modes (`Text`, `OwnText`, `Attribute`, `ElementExists`, `AttributeExists`, `Adjacent`), `extract_fields`/`fields`, `html`, `outer_html`, `tag_name`, `classes`, `attrs(data_only)`, `xpath`, `xpath3`, `table`, `extract_meta` |
| **Text, Encoding & Unicode** | [`text_normalization_and_unicode.dsl`](text_normalization_and_unicode.dsl), [`text_escaping_and_cleaning.dsl`](text_escaping_and_cleaning.dsl), [`text_lists_base64_and_mapping.dsl`](text_lists_base64_and_mapping.dsl) | [`.html`](text_and_encoding.html), [`.py`](text_and_encoding.py) | `trim`, `squish`, `normalize_whitespace`, `lower`, `upper`, `unescape`, `unescape_html`, `replace` (scalar & mapping), `split`, `join`, `prepend`, `append`, `drop_empty`, `substring`, `strip_comments`, `strip_tags`, `base64_encode`, `base64_decode`, `hash` (`fnv1a64`, `sha256`), `slugify`, `map_values`, `unicode_normalize` (`NFC`/`NFD`/`NFKC`/`NFKD`), `strip_accents` |
| **URLs, Srcset & CSS URL** | [`urls_and_query_params.dsl`](urls_and_query_params.dsl), [`srcset_and_css_urls.dsl`](srcset_and_css_urls.dsl) | [`.html`](urls_and_web.html), [`.py`](urls_and_web.py) | `resolve_url`, `url_part` (`scheme`/`host`/`port`/`path`/`query`/`fragment`), `query_param`, `query_params`, `strip_query`, `filter_urls`, `parse_srcset`, `css_url` |
| **Regex, Aho-Corasick & Fuzzy** | [`regex_extraction_and_filtering.dsl`](regex_extraction_and_filtering.dsl), [`aho_corasick_and_fuzzy.dsl`](aho_corasick_and_fuzzy.dsl) | [`.html`](regex_ac_and_fuzzy.html), [`.py`](regex_ac_and_fuzzy.py) | `regex_replace`, `regex_filter`, `regex_extract`, `regex_find_all`, `regex_groups` (`i`/`s`/`m` flags), `filter`, `extract_many`, `fuzzy_filter`, `similarity`, `AhoCorasick` (`add_pattern`, `build`, `clear`, `pattern_count`, `contains_any`, `has_all`, `search`) |
| **Slicing, Types & Dates** | [`slicing_and_ordering.dsl`](slicing_and_ordering.dsl), [`types_and_numbers.dsl`](types_and_numbers.dsl), [`dates_and_durations.dsl`](dates_and_durations.dsl) | [`.html`](slicing_ordering_and_types.html), [`.py`](slicing_ordering_and_types.py) | `first`, `last`, `nth`, `slice`, `unique`, `sort`, `reverse`, `default`, `set(type=...)`, `append_to`, `count`, `parse_number`, `parse_duration`, `to_int`, `to_float`, `to_bool`, `parse_date` |
| **JS Digger, JSON, CSV & Queries** | [`js_json_and_csv.dsl`](js_json_and_csv.dsl), [`jsonpath_projection.dsl`](jsonpath_projection.dsl), [`jmespath_projection.dsl`](jmespath_projection.dsl) | [`.html`](js_digger_json_csv_queries.html), [`.py`](js_digger_json_csv_queries.py) | `js_block`, `js_blocks`, `parse_js`, `json_parse`, `json_dump`, `csv_parse`, `csv_dump`, `jmespath`, `jsonpath`, `json_schema`, Python `jmespath_eval`, `jsonpath_query`, `json_schema_validate` |
| **Dict Transforms, Merging & Docs** | [`dict_transforms.dsl`](dict_transforms.dsl), [`dict_flatten_and_pick.dsl`](dict_flatten_and_pick.dsl) | [`.html`](dict_transforms_and_merging.html), [`.py`](dict_transforms_and_merging.py) | `flatten`, `pick_keys`, `omit_keys`, `rename_keys`, `merge_keys`, `zip_keys`, `coalesce_keys`, `cast_keys`, `sort_array`, `deduplicate`, `debug`, `InsertMode` (`Default`, `Overwrite`, `AppendToList`, `UniqueSuffix`, `Merge`), `InsertOptions`, `Document`, `from_document`, `from_file`, `scrape_many` |
| **CLI All Modes** | — | [`cli_all_modes.py`](cli_all_modes.py) | `-h`/`--help`, `-s`/`--selector`, `-d`/`--dsl`, `-n`/`--name`, `--html` (file, string, `-`, stdin), `-c`/`--compact`, `--batch`, `--ndjson`, `--json-array`, `--json-object`, `--out-dir`, `-j`/`--threads` |

---

## Automated Coverage Verification

`tests/test_examples.py` automatically verifies that:
- Every topic script exits with code `0`.
- Every split `.dsl` snippet defines exactly `1` scraper (with `dsl_include.dsl` as the sole `include()` exception).
- **100%** of methods, enums, and functions in `scraperpy-stubs/_scraperpy.pyi` are exercised in the topic `.py` scripts.
- **100%** of DSL steps, aliases, and control-flow keywords in `docs/dsl-and-steps.md` are exercised in the split `.dsl` snippets.
