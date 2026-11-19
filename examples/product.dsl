# Example DSL file for examples/sample.html
let title_sel = "h1.product-title"

product = Scraper()
  .push(title_sel).text().normspace().set("title")
  .push(".price").text().regex_extract("([0-9]+\\.[0-9]+)", 1).set("price")
  .push(".price").attr("data-currency").set("currency")
  .push(".badge").text().trim().set("badges")
  .push("ul.specs li").text().trim().set("specs")
  .push("a.docs-link").attr("href").prepend("https://example.com").set("manual_url")
  .push("script").text().block_seeker("window.__PRODUCT_STATE__ =").parse("state", true)

# A second scraper showing JMESPath post-processing on the extracted state
summary = Scraper()
  .push(title_sel).text().normspace().set("title")
  .push("script").text().block_seeker("window.__PRODUCT_STATE__ =").parse("state", true)
  .jmespath("{title: title, sku: state.sku, rating: state.rating}")
