// @playground id=ex_03_text_encoding label="03: Text, Encoding & Unicode" scraper=text_and_encoding
text_and_encoding = Scraper()
  | $("h1.heading", "case_and_ws") {
      text | trim() -> "trimmed"
      | text | squish() -> "squished"
      | text | normalize_whitespace() -> "norm_ws"
      | text | squish() | lower() -> "lowered"
      | text | squish() | upper() -> "uppered"
      | text | squish() | substring(0, 5) -> "sub_prefix"
      | text | squish() | substring(-6) -> "sub_suffix"
      | text | squish() | unicode_normalize("NFC") -> "nfc"
      | text | squish() | unicode_normalize("NFD") -> "nfd"
      | text | squish() | unicode_normalize("NFKC") -> "nfkc"
      | text | squish() | unicode_normalize("NFKD") | strip_accents() -> "ascii_folded"
      | text | squish() | strip_accents() | slugify("_", 20) -> "slug"
      | text | squish() | hash("fnv1a64") -> "fnv1a64"
      | text | squish() | hash("sha256") -> "sha256"
    }
  | $("div.escaped", "escapes") {
      text | unescape(url_decode=true) -> "full_unescape"
      | text | unescape(url_decode=false) -> "no_url_decode"
    }
  | $("div.entities", "html_ent") {
      text | unescape_html() -> "decoded"
    }
  | $("div.code-text", "comments_and_replace") {
      text | strip_comments(multiline=true, single_line=true) | squish -> "no_comments"
      | text | strip_comments(multiline=true, single_line=false) | squish -> "keep_single_line"
      | text | strip_comments() | squish | replace("hello", "hi", first_only=true) -> "replaced_one"
      | text | strip_comments() | squish | replace({"hello": "greetings", "world": "earth"}) -> "replaced_map"
    }
  | $("div.html-snippet", "tags") {
      html() | strip_tags() | squish -> "stripped_tags"
    }
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
