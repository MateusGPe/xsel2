// =============================================================================
// E-Commerce Catalog & Multi-Variant Extraction Pipeline
// =============================================================================

let origin = "https://audiohub.example"

catalog = Scraper()
  | $("div.product-item", "items") {
      @"data-id" -> "product_id"
      | @"data-available" | to_bool(key="available")
      | find("h3.name") | clean -> "name"
      | find(".price", Attribute, "data-usd") | to_float(key="price_usd")
      | find_all(".badge") | clean -> "badges"
      | find_all(".variants .color", Attribute, "data-color") -> "available_colors"
      | find_all(".variants .color", Attribute, "data-sku") -> "variant_skus"
      | find("a.p-link", Attribute, "href") | url_join(origin) -> "url"
    }
  | $("nav.pagination") {
      find("span.current") | clean -> "page_status"
      | find("a.next-page", Attribute, "href") | url_join(origin) -> "next_page_url"
    }
