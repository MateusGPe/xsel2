# Example DSL file for examples/sample.html
let title_sel = "h1.product-title"

product = Scraper()
  .select(title_sel).text().squish().set("title")
  .select(".price").text().regex_extract("([0-9]+\\.[0-9]+)", 1).set("price")
  .select(".price").attr("data-currency").set("currency")
  .select(".badge").text().trim().set("badges")
  .select("ul.specs li").text().trim().set("specs")
  .select("a.docs-link").attr("href").prepend("https://example.com").set("manual_url")
  .select("script").text().js_block("window.__PRODUCT_STATE__ =").parse_js("state", true)

# A second scraper showing JMESPath post-processing on the extracted state
summary = Scraper()
  .select(title_sel).text().squish().set("title")
  .select("script").text().js_block("window.__PRODUCT_STATE__ =").parse_js("state", true)
  .jmespath("{title: title, sku: state.sku, rating: state.rating}")
