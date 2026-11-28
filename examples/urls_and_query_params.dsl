// @playground id=urls_and_query_params label="URLs, Query Params & Filtering" html=urls_and_web.html scraper=urls_and_queries
let base = "https://shop.example.com:443/catalog/index.html"

urls_and_queries = Scraper()
  | $("a.rel", "url_parts") {
      @href | resolve_url(base) -> "resolved"
      | @href | resolve_url(base) | url_part("scheme") -> "scheme"
      | @href | resolve_url(base) | url_part("host") -> "host"
      | @href | resolve_url(base) | url_part("port") -> "port"
      | @href | resolve_url(base) | url_part("path") -> "path"
      | @href | resolve_url(base) | url_part("query") -> "query"
      | @href | resolve_url(base) | url_part("fragment") -> "fragment"
      | @href | resolve_url(base) | query_param("q", "none") -> "q_decoded"
      | @href | resolve_url(base) | query_param("missing", "fallback") -> "q_missing"
      | @href | resolve_url(base) | strip_query(keep_params=["ref", "q"]) -> "kept_query"
      | @href | resolve_url(base) | strip_query(drop_params=["utm_*"]) -> "dropped_utm"
      | @href | resolve_url(base) | strip_query() -> "stripped_all"
    }
  | $("a.brackets", "bracket_params") {
      @href | query_params({"ref": "referrer", "filter[color]": "color", "missing": "opt"}, default_val="n/a")
    }
  | $("nav.links", "filtered_links") {
      find_all("a", Attribute, "href")
      | resolve_url(base)
      | filter_urls(allowed_domains=["example.com"], blocked_domains=["evil.org"], allowed_schemes=["https"]) -> "safe_example_urls"
      | find_all("a", Attribute, "href")
      | resolve_url(base)
      | filter_urls(same_origin_of=base) -> "same_origin_urls"
    }
