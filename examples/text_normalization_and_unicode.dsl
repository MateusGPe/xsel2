// @playground id=text_normalization_and_unicode label="Text Normalization & Unicode" html=text_and_encoding.html scraper=text_norm_unicode
text_norm_unicode = Scraper()
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
