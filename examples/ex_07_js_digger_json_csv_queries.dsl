// @playground id=ex_07_js_json_csv label="07: JS Digger, JSON, CSV & Queries" scraper=js_json_csv
js_json_csv = Scraper()
  | $("script#hydration", "js") {
      text | js_block("window.__DATA__ =") | parse_js("data", simplify=true)
      | text | js_blocks({"window.__DATA__ =": "d", "window.__EXTRA__ =": "extra"})
    }
  | $("div.json-commented", "json_ops") {
      text | json_parse("cfg", allow_comments=true, allow_trailing_comma=true, allow_inf_and_nan=false, lossless_number=false, max_nesting_depth=64, strict=true)
      | json_dump(pretty=true, indent=2, source_key="cfg") -> "cfg_pretty"
    }
  | $("div.json-lenient", "json_ops") {
      text | json_parse("lenient_obj", lenient=true)
    }
  | $("pre.csv-block", "csv_ops") {
      text | trim | csv_parse("rows", assume_header=true, delimiter=",", trim=true, strict=true)
      | csv_dump(assume_header=true, delimiter=";", trim=true, source_key="rows") -> "csv_semicolon"
    }
  | finalize {
      json_schema({
        "type": "object",
        "required": ["js", "json_ops", "csv_ops"]
      }, strict=false, error_key="_schema_errors")
    }

query_projections = Scraper()
  | $("script#hydration") {
      text | js_block("window.__DATA__ =") | parse_js("data")
    }
  | $("pre.csv-block") {
      text | trim | csv_parse("rows")
    }
  | finalize {
      jsonpath("$.rows[?(@.score == 95)].name")
    }

jmespath_projection = Scraper()
  | $("script#hydration") {
      text | js_block("window.__DATA__ =") | parse_js("data")
    }
  | finalize {
      jmespath("{sku: data.sku, price: data.price, nested: data.payload.nested}")
    }
