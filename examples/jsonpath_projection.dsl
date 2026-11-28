// @playground id=jsonpath_projection label="JSONPath Projection" html=js_digger_json_csv_queries.html scraper=query_projections
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
