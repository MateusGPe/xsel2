// @playground id=jmespath_projection label="JMESPath Projection" html=js_digger_json_csv_queries.html scraper=jmespath_projection
jmespath_projection = Scraper()
  | $("script#hydration") {
      text | js_block("window.__DATA__ =") | parse_js("data")
    }
  | finalize {
      jmespath("{sku: data.sku, price: data.price, nested: data.payload.nested}")
    }
