// @playground id=srcset_and_css_urls label="Srcset & CSS URLs" html=urls_and_web.html scraper=srcset_and_css
let base = "https://shop.example.com:443/catalog/index.html"

srcset_and_css = Scraper()
  | $("img.hero", "srcset_demo") {
      @srcset | parse_srcset("largest") | resolve_url(base) -> "largest"
      | @srcset | parse_srcset("smallest") | resolve_url(base) -> "smallest"
      | @srcset | parse_srcset("all") -> "all_urls"
    }
  | $("img.dpr", "srcset_dpr") {
      @srcset | parse_srcset("best") -> "best_dpr"
    }
  | $("style", "css_urls") {
      text | css_url() | resolve_url(base) -> "from_style_tag"
    }
  | $("div.inline-bg", "css_urls") {
      @style | css_url() | resolve_url(base) -> "from_inline_style"
    }
