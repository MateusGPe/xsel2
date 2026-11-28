# Representative real-world HTML scraping benchmark for xsel2
# Exercises CSS selectors, text/attr extraction, squish/trim, regex_extract,
# multi-element list deduplication (drop_empty + unique), embedded <script> JS parsing,
# and JMESPath projection.

real_scrape := Scraper()
  .select("html") {
    find_all("script", Value.Text) | js_block("var PAGE_STATE =") | parse_js("PAGE_STATE", true)
    | fields({
      page_title: "title",
      og_title: { sel: "meta[property=\"og:title\"]", attr: "content" },
      canonical_url: { sel: "link[rel=\"canonical\"]", attr: "href", squish: false },
      heading: "h1",
      rating_pct: ".rating-percent .percent, .media-rating .percent"
    })
    | find("link[rel=\"canonical\"]", Value.Attribute, "href") | regex_extract("item_id=([a-zA-Z0-9]+)", 1) -> "item_id"
    | find(".rating-percent .percent, .media-rating .percent", Value.Text) | regex_extract("([0-9]+)", 1) -> "rating_pct"
    | find_all(".tag-list a, .category-list a", Value.Text) | squish | drop_empty | unique -> "tags"
  }
  .finalize {
    jmespath('{
      page_title: page_title,
      og_title: og_title,
      canonical_url: canonical_url,
      item_id: item_id,
      heading: heading,
      rating_pct: rating_pct,
      tags: tags,
      item_num_id: PAGE_STATE.item_id,
      item_key: PAGE_STATE.key,
      segment: PAGE_STATE.segment
    }')
  }
