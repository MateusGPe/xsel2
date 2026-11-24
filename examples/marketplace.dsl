// =============================================================================
// xsel2 - Comprehensive Marketplace Example
// =============================================================================
// Demonstrates:
//   - DSL variables, pipe sugar (|, ->, ??, @attr, $(...) { ... })
//   - DOM, text, regex, and slicing steps
//   - Lenient JS/JSON digger (js_block, js_blocks, parse_js with simplify=true)
//   - Relaxed JSON-LD (json_parse) and CSV (csv_parse)
//   - JSONPath (jsonpath), JSON Schema (json_schema), and JMESPath normalization.
// =============================================================================

let card_sel = ".product-card"
let json_ld_sel = "script[type='application/ld+json']"

// 1. Full extraction + lenient digger + JSON transformations & validation
catalog_full = Scraper()
  | $("head title", "storefront") {
      text | unescape | clean -> "title"
    }
  | $("link[rel='canonical']", "storefront") {
      @href -> "canonical_url"
    }
  | $(".promo-banner", "storefront") {
      text | unescape | clean | sub("Spring\\s+Sale:", "Featured:") -> "promo"
    }
  | $("nav.breadcrumbs", "storefront") {
      find_all("a", Value.Text) | unescape | clean | dedup | slice(1, 2) -> "breadcrumbs"
    }
  | $(card_sel, "products") {
      @"data-sku" -> "sku"
      | @"data-category" -> "category"
      | @"data-in-stock" -> "in_stock"
      | find("h2.name", Value.Text) | clean -> "name"
      | find("span.price", Value.Text) | extract("([0-9]+\\.[0-9]+)", 1) ?? "0.00" -> "price"
      | find_all("span.badge", Value.Text) | clean | dedup -> "badges"
      | find_all("ul.specs li", Value.Text) | clean -> "specs"
      | find("div.seller", Value.OwnText) | clean -> "seller"
      | find("div.seller span.rating", Value.Text) | clean -> "rating"
    }
  | $(json_ld_sel, "json_ld") {
      text | json_parse(allow_comments=true, allow_trailing_comma=true, strict=false)
    }
  | $("script#shipping-rates") {
      text | trim | csv_parse("shipping_rates", assume_header=true, delimiter=",", trim=true, strict=false)
    }
  | $("script") {
      text | js_block("window.__CATALOG_STATE__ =") | parse_js("hydration", true)
      | text | js_blocks(["window.__PROMO_CONFIG__"])
    }
  // Top-level JSON transformations & validation
  | omit_keys(["apiSecret", "wholesaleCost"])
  | deduplicate()
  | sort_array(true)
  | json_schema({
      "type": "object",
      "required": ["storefront", "products", "hydration", "shipping_rates"],
      "properties": {
        "products": {"type": "array", "minItems": 1}
      }
    }, strict=false, error_key="_schema_errors")


// 2. Normalized summary via JMESPath post-processing
catalog_summary = Scraper()
  | $("head title", "storefront") { text | unescape | clean -> "title" }
  | $(card_sel, "products") {
      @"data-sku" -> "sku"
      | @"data-in-stock" -> "in_stock"
      | find("h2.name", Value.Text) | clean -> "name"
      | find("span.price", Value.Text) | extract("([0-9]+\\.[0-9]+)", 1) ?? "0.00" -> "price"
      | find("div.seller", Value.OwnText) | clean -> "seller"
    }
  | $(json_ld_sel, "json_ld") {
      text | json_parse(allow_comments=true, allow_trailing_comma=true, strict=false)
    }
  | $("script#shipping-rates") {
      text | trim | csv_parse("shipping_rates", assume_header=true, delimiter=",", trim=true, strict=false)
    }
  | $("script") {
      text | js_block("window.__CATALOG_STATE__ =") | parse_js("hydration", true)
    }
  | jmespath("{
      store: storefront.title,
      total_items: json_ld.numberOfItems,
      warehouse: hydration.fulfillment.warehouse,
      api_base: hydration.apiBase,
      in_stock_products: products[?in_stock == 'true'].{
        sku: sku,
        name: name,
        price: price,
        seller: seller
      },
      shipping_carriers: shipping_rates[*].carrier
    }
  ")


// 3. JSONPath query scraper
product_skus = Scraper()
  | $(card_sel, "products") { @"data-sku" -> "sku" }
  | jsonpath("$.products[*].sku")
