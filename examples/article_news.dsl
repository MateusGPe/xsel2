// =============================================================================
// News Article & JSON-LD Extraction Pipeline
// =============================================================================

let site_base = "https://techpulse.dev"

article = Scraper()
  .$("article.post-entry") {
    find("h1.headline") | clean -> "title"
    | find(".byline .author") | clean -> "author"
    | find(".byline time.pub-date", Attribute, "datetime") -> "published_at"
    | find(".byline .read-time") | clean -> "reading_time"
    | find_all("nav.breadcrumbs a") | clean -> "breadcrumbs"
    | find_all(".post-tags a.tag") | clean -> "tags"
    | find(".post-body p.lead") | clean -> "summary"
  }
  .$("script[type='application/ld+json']") {
    text | json_parse("json_ld", strict=false)
  }
  | omit_keys(["json_ld.@context"])
