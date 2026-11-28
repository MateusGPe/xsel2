// @playground id=text_escaping_and_cleaning label="Text Escaping, Comments & Tags" html=text_and_encoding.html scraper=text_escaping_cleaning
text_escaping_cleaning = Scraper()
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
