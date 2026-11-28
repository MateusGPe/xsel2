// @playground id=text_lists_base64_and_mapping label="Text Lists, Base64 & Mapping" html=text_and_encoding.html scraper=text_lists_b64_map
text_lists_b64_map = Scraper()
  | $("div.csv-line", "lists") {
      text | split(",", keep_empty_parts=true) | drop_empty(also_trim=true) | prepend("[") | append("]") | join(" | ") -> "cleaned_joined"
      | text | split(",", keep_empty_parts=false) -> "split_nonempty"
    }
  | $("div.b64-std", "base64") {
      text | trim | base64_decode(urlsafe=false) -> "decoded_std"
      | text | trim | base64_decode() | base64_encode(urlsafe=false) -> "roundtrip_std"
      | text | trim | base64_decode() | base64_encode(urlsafe=true) -> "encoded_urlsafe"
    }
  | $("div.b64-url", "base64") {
      text | trim | base64_decode(urlsafe=true) -> "decoded_urlsafe"
    }
  | $("ul.statuses", "mapping") {
      find_all("li", Text) | map_values({"IN_STOCK": "available", "BACKORDER": "delayed"}, default_val="other") -> "mapped"
    }
