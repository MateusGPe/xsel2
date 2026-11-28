// @playground id=ex_01_dsl_syntax label="01: DSL Syntax & Flow" scraper=syntax_and_aliases
# Hash single-line comment
// C++ style single-line comment
/* Multi-line
   block comment */

let base_url = "https://shop.example"
let title_sel = "h1.brand"
let escaped_var = "literal \${base_url}"

// Reusable pipe fragment + use()
pipe kv_pair = {
  find("dt", Text) -> "@key"
  | find("dd", Text) -> "@value"
}

// Base scraper for include()
base_meta = Scraper()
  | $(title_sel, "meta") {
      text | squish -> "brand"
    }

// Demonstrates declarations (=, :, :=, implicit), separators (. | |>),
// shorthands ($ @ ?? [0] [-1] -> >> as), all 17 aliases, and multiline strings.
syntax_and_aliases: ScraperPy()
  .include(base_meta)
  |> $("header.site-header", "header") {
       // Shorthands: @ident, @"attr", @("attr"), @[fallback list]
       // Note: bare @ident allows [A-Za-z_][A-Za-z0-9_]*; hyphenated attrs use @"data-env"
       @class -> "header_class"
       | @"data-env" >> "env"
       | @("data-env") as "env_alias"
       | find("span.subtitle", Attribute, ["data-missing", "data-alt"]) -> "sub_alt"
       | find("span.missing", Text) ?? "Default Fallback" -> "fallback_val"
       | find("span.subtitle", Text) | strip | clean | normalize_whitespace -> "subtitle_clean"
       | find("span.subtitle", Text) | sub("Fast", "Ultra-Fast") | extract("(Ultra-Fast)", 1) -> "extracted"
       | find("span.subtitle", Text) | grep("Fast") -> "grepped"
       | find("h1.brand", Text) | inner_html -> "noop_text_check"
     }
  . $("article.card[data-id='101']", "card") {
       @"data-id" | as("id", "int")
       | find("h2.name", Text) | clean -> "name"
       | find(".price", Text) | to_number(key="price")
       | find("a.cta", Attribute, "href") | url_join(base_url) -> "cta_url"
       | find_all(".tags span", Text) | strip | dedup | limit(2) -> "top_tags"
       | find_all(".tags span", Text) | strip | [0] -> "first_tag"
       | find_all(".tags span", Text) | strip | [-1] -> "last_tag"
       | find("div.raw-json", Text) | json("parsed_json")
       | find("script", Text) | extract_js_block("window.__CFG__ =") | parse_js("cfg")
       | find_all("script", Text) | extract_js_blocks(["window.__CFG__ =", "window.__META__ ="], {"window.__CFG__ =": "cfg_alias", "window.__META__ =": "meta_alias"})
       | fields({ title: { sel: "h2.name", squish: true }, price_raw: { sel: ".price", trim: true } })
       | find("h2.name", Text) | clean | branch {
           upper -> "name_upper"
         } | lower -> "name_lower"
       .(.find(".badge", Text) | upper -> "grouped_badge")
     }
  | $("dl.specs", "specs_list") {
      // Nested relative $ block + @key/@value
      each("dt", "") {
        text -> "dt_only"
      }
    }
  | $("dl.specs", "specs_kv") {
      use(kv_pair)
    }
  | $("div.nested-groups div.group", "groups") {
      @"data-group" -> "group"
      | $("li", "items") {
          @"data-code" -> "code"
          | text -> "label"
        }
    }
  | finalize {
      jmespath("{
        brand: meta.brand,
        env: header.env,
        card_id: card.id,
        card_name_upper: card.name_upper,
        card_name_lower: card.name_lower,
        escaped: `\"${escaped_var}\"`
      }")
    }

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

// Implicit root scraper + @key/@value collapsing
implicit_root = .select("dl.specs") {
  find("dt", Text) -> "@key"
  | find("dd", Text) -> "@value"
}
