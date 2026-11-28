// @playground id=dsl_include label="DSL Scraper Include" html=dsl_syntax_and_composition.html scraper=composed_scraper
let title_sel = "h1.brand"

// Base scraper for include()
base_meta = Scraper()
  | $(title_sel, "meta") {
      text | squish -> "brand"
    }

composed_scraper = Scraper()
  .include(base_meta)
  | $("header.site-header", "header") {
      @"data-env" -> "env"
    }
