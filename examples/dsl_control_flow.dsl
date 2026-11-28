// @playground id=dsl_control_flow label="DSL Control Flow & Guards" html=dsl_syntax_and_composition.html scraper=control_flow
// Control flow, guards, save/restore, tee, when_exists/unless_exists/when_match
control_flow := Scraper()
  | $("article.card", "filtered_cards") {
      skip_if_exists("span.skip-marker")
      | find("h2.name", Text) | skip_if_empty()
      | save("orig_name")
      | tee {
          upper -> "upper_name"
        }
      | restore("orig_name")
      | lower -> "lower_name"
      | when_exists("span.badge") {
          find("span.badge", Text) -> "badge"
        }
      | unless_exists("span.out-of-stock") {
          find(".price", Text) | when_match("\\$19", flags="i") {
            prepend("USD ") -> "matched_price"
          }
        }
    }
  | $("div.abort-zone", "abort_demo") {
      find("span.normal", Text) -> "normal"
      | abort_if_exists("span.maintenance", key="aborted_reason", reason="maintenance_mode")
    }
  | $("header.site-header", "never_reached_after_abort") {
      find("h1.brand", Text) -> "brand"
    }
