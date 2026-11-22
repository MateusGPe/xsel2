// =============================================================================
// xsel2 - Complete Steps, Features & Algorithms Showcase
// =============================================================================
// Run any scraper against examples/all_steps_showcase.html:
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n dom_and_text --html examples/all_steps_showcase.html
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n urls_dates_unicode --html examples/all_steps_showcase.html
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n algorithms_and_matching --html examples/all_steps_showcase.html
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n types_tables_and_dict --html examples/all_steps_showcase.html
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n serialization --html examples/all_steps_showcase.html
//   ./build/xsel2 -d examples/all_steps_showcase.dsl -n json_transforms --html examples/all_steps_showcase.html
// =============================================================================

let base_origin = "https://shop.example.com"

// 1. DOM inspection, text normalization, slicing, and ordering
dom_and_text = Scraper()
  | $("header#hero", "hero") {
      tag_name() -> "tag"
      | classes() -> "classes"
      | attrs(data_only=true)
      | find("h1.main-title", Value.Text) | trim | squish | lower -> "title_lower"
      | find("h1.main-title", Value.Text) | squish | upper -> "title_upper"
      | find("p.tagline", Value.Text) | strip_comments() | squish | unescape() -> "tagline_clean"
      | find("p.tagline", Value.OwnText) | strip_comments() | squish -> "tagline_own_text"
      | find("h1.main-title", Value.AttributeExists, "class") -> "has_title_class"
      | find("h1.main-title", Value.ElementExists) -> "has_title_el"
    }
  | $("p.tagline", "tagline_dom") {
      html(outer=false) | strip_tags() | strip_comments() | squish -> "stripped"
      | outer_html() -> "outer"
    }
  | $("article.item", "items") {
      @"data-sku" -> "sku"
      | find("h2.title", Value.Text) | replace("Pro 75", "Pro-75") | replace("Wireless Ergonomic Mouse", "Ergo Mouse") -> "name"
      | find("h2.title", Value.Attribute, "data-status") ?? "In Stock" -> "status"
      | find_all("ul.tags li", Value.Text) | drop_empty(true) | unique(true) | sort(ascending=true, numeric=false) | reverse() -> "tags_sorted_desc"
      | find_all("ul.tags li", Value.Text) | unique(true) | first(2) -> "first_two_tags"
      | find_all("ul.tags li", Value.Text) | unique(true) | last(1) -> "last_tag"
      | find_all("ul.tags li", Value.Text) | nth(0) -> "first_tag"
      | find_all("ul.tags li", Value.Text) | slice(0, 1) | join(" | ") | prepend("[") | append("]") -> "tag_summary"
      | find_all("h2.title", Value.Attribute, "data-badge") | default("fallback-badge") -> "coalesced"
      | find_all("h2.title", Value.Attribute, "data-default") | default("default-badge") -> "defaulted"
    }
  | $("span.label", "adjacent_labels") {
      text | squish -> "label"
      | find_all("span.next-label", Value.Adjacent) | squish -> "next_label"
    }
  | $("head title", "title_parts") {
      text | squish | split("—") | trim | nth(1) | substring(0, 15) -> "subtitle_prefix"
    }
  | $("html", "xpath_skus") {
      xpath("//section[@id='catalog']/article/@data-sku") -> "skus"
    }


// 2. URLs, Dates, Unicode normalization, Encoding, and Hashing
urls_dates_unicode = Scraper()
  | $("a.cta-link", "link") {
      @href | resolve_url(base_origin) -> "absolute_url"
      | @href | resolve_url(base_origin) | url_part("host") -> "host"
      | @href | resolve_url(base_origin) | url_part("path") -> "path"
      | @href | resolve_url(base_origin) | query_param("currency", "EUR") -> "currency"
      | @href | resolve_url(base_origin) | strip_query(["currency"]) -> "clean_url"
    }
  | $("h1.main-title", "text_norm") {
      text | squish | unicode_normalize("NFC") -> "nfc"
      | text | squish | unicode_normalize("NFKD") | strip_accents() -> "ascii_folded"
      | text | squish | strip_accents() | slugify("-", 32) -> "slug"
      | text | squish | hash("fnv1a64") -> "hash_fnv1a64"
      | text | squish | hash("sha256") -> "hash_sha256"
    }
  | $("time.published", "dates") {
      @datetime | parse_date(output="iso", tz="UTC") -> "iso"
      | @datetime | parse_date(output="date", tz="UTC") -> "date_only"
      | @datetime | parse_date(output="unix", tz="UTC") -> "unix_sec"
      | @datetime | parse_date(output="unix_ms", tz="UTC") -> "unix_ms"
    }
  | $("span.updated", "dates") {
      text | parse_date(fmt="%Y/%m/%d %H:%M:%S", output="%Y-%m-%d", tz="UTC") -> "custom_fmt"
    }
  | $("div.b64", "encoding") {
      text | trim | base64_decode() -> "decoded"
      | text | trim | base64_decode() | base64_encode(urlsafe=true) -> "reencoded_urlsafe"
    }


// 3. Algorithms: RE2 Regex, Aho-Corasick multi-pattern, Jaro-Winkler & Levenshtein fuzzy matching
algorithms_and_matching = Scraper()
  | $("article.item", "regex_and_ac") {
      find("h2.title", Value.Text) | regex_replace("\\s+", "_", flags="i") -> "snake_title"
      | find("span.price", Value.Text) | regex_extract("([0-9,]+\\.[0-9]{2})", 1) -> "raw_price"
      | find_all("ul.tags li", Value.Text) | regex_filter("^(USB-C|Bluetooth)$") -> "connectivity_tags"
      | find_all("ul.tags li", Value.Text) | filter(["USB-C", "Thunderbolt"]) -> "ac_filtered_tags"
      | find_all("ul.tags li", Value.Text) | replace({"USB-C": "Type-C", "Hot-Swap": "Hotswap"}) -> "ac_replaced_tags"
      | find("h2.title", Value.Text) | extract_many(["Mechanical", "Wireless", "USB-C", "Dock"], unique=true) -> "ac_extracted_keywords"
    }
  | $("ul.models", "fuzzy") {
      find_all("li", Value.Text) | fuzzy_filter("Mechanical Keyboard", threshold=0.75, algo="jaro_winkler", case_sensitive=false, sort_by_score=true) -> "jw_matches"
      | find_all("li", Value.Text) | fuzzy_filter("Mechanical Keyboard", threshold=0.70, algo="levenshtein", case_sensitive=false, sort_by_score=true) -> "lev_matches"
      | find_all("li", Value.Text) | similarity("Mechanical Keyboard", algo="jaro_winkler", key="jw_scores")
    }
  | $("ul.models", "fuzzy") {
      find_all("li", Value.Text) | similarity("Mechanical Keyboard", algo="levenshtein", key="lev_scores")
    }


// 4. Typed parsing, HTML tables, metadata extraction, and dictionary manipulation
types_tables_and_dict = Scraper()
  | extract_meta("page_meta", include_json_ld=true)
  | $("table.comparison") {
      table(key="comparison_rows", header=true, normalize_headers=true, trim_cells=true)
    }
  | $("article.item", "products") {
      @"data-sku" -> "sku"
      | @"data-stock" | to_bool(key="in_stock", default_val=false)
      | @"data-rating" | to_float(key="rating_num")
      | find("span.price", Value.Text) | parse_number(decimal=".", thousands=",", allow_float=true, key="price_num")
      | find_all("ul.tags li", Value.Text) | unique(true) | count("tag_count")
      | find_all("ul.tags li", Value.Text) | append_to("collected_tags", allow_empty=false)
      | find("span.price", Value.Text) | parse_number(allow_float=false) | to_int(key="price_int")
      | @"data-rating" | set("rating_auto", type="auto")
      | find("h2.title", Value.Text) -> "temp_name"
      | flatten()
    }
  | $("table.comparison tbody tr", "zip_demo") {
      find_all("td", Value.Text) | nth(0) -> "k"
      | find_all("td", Value.Text) | nth(2) -> "v"
    }
  | $("section#catalog", "numeric_sort") {
      find_all("article.item span.price", Value.Text) | parse_number() | sort(ascending=true, numeric=true) -> "prices_asc"
    }
  | cast_keys({"page_meta.json_ld.numberOfItems": "int"})
  | rename_keys({"temp_name": "title"})
  | pick_keys(["page_meta", "comparison_rows", "products", "numeric_sort"])
  | omit_keys(["page_meta.meta"])
  | merge_keys(["numeric_sort"], "pricing_summary")
  | sort_array(true)


// 5. Serialization (JSON, CSV) + zip_keys
serialization = Scraper()
  | $("div.json-blob", "parsed") {
      text | strip_comments() | json_parse("config", allow_comments=true, allow_trailing_comma=true, strict=false)
      | json_dump(pretty=false, source_key="config") -> "config_json_str"
    }
  | $("pre.csv-blob", "csv") {
      text | trim | csv_parse("rows", assume_header=true, delimiter=",", trim=true, strict=true)
      | csv_dump(assume_header=true, delimiter=";", trim=true, source_key="rows") -> "semicolon_csv"
    }
  | $("script") {
      text | js_block("window.__SHOWCASE_STATE__ =") | parse_js("state", simplify=true)
      | text | js_blocks(["window.__PROMO__"])
    }
  | $("section#catalog", "zipped") {
      find_all("article.item", Value.Attribute, "data-sku") -> "skus"
      | find_all("article.item", Value.Attribute, "data-rating") -> "ratings"
      | zip_keys("skus", "ratings", "sku_to_rating")
    }


// 6. Top-level JSON transforms: flatten, Schema, JMESPath, JSONPath
json_transforms = Scraper()
  | $("script") {
      text | js_block("window.__SHOWCASE_STATE__ =") | parse_js("state", simplify=true)
    }
  | omit_keys(["secretToken", "experimental"])
  | json_schema({
      "type": "object",
      "required": ["state"],
      "properties": {
        "state": {"type": "object"}
      }
    }, strict=true)
  | flatten()
  | deduplicate()
  | jmespath("{store: state_store, build: state_build, fast: state_flags_fastMode}")
  | jsonpath("$.*")


// 7. Debug trace example (prints values/dict/node to stderr when invoked)
debug_trace = Scraper()
  | $("h1.main-title") {
      text | squish | debug("SHOWCASE: ", print_dict=false, print_node=false) -> "title"
    }
