// @playground id=ex_02_dom_selection label="02: DOM, XPath & Tables" scraper=dom_and_values
dom_and_values = Scraper()
  | $("section#dom-box", "box") {
      tag_name() -> "tag"
      | classes() -> "classes"
      | attrs(data_only=true)
      | find("p.intro", Value.Text, squish=true) -> "full_text"
      | find("p.intro", Value.OwnText, squish=true) -> "own_text"
      | find("a.doc-link", Value.Attribute, ["data-missing", "href"]) -> "href"
      | find("a.doc-link", Value.ElementExists) -> "link_exists"
      | find("a.doc-link", Value.AttributeExists, "data-alt") -> "has_data_alt"
      | find("div.rich", Value.Text) -> "rich_text"
    }
  | $("ul.xml-items", "box") {
      find_all("li", Value.Text) -> "all_li"
    }
  | $("span.label", "box") {
      find("span.next-label", Value.Adjacent) -> "adjacent_text"
    }
  | $("div.rich", "html_inspect") {
      html(outer=false) -> "inner"
      | html(outer=true) -> "outer_flag"
      | outer_html() -> "outer"
    }
  | $("p.intro", "text_modes") {
      text(own_text=false) | squish -> "text_full"
      | text(own_text=true) | squish -> "text_own"
    }
  | $("a.doc-link", "attr_list") {
      attr(["data-missing", "data-alt", "href"]) -> "first_found_attr"
    }
  | $("section#dom-box", "fields_demo") {
      extract_fields({
        heading: { selector: "p.intro", type: "own_text", squish: true },
        doc_href: { sel: "a.doc-link", type: "attr", attr: "href", trim: true },
        missing_opt: { sel: ".missing", default: "N/A" },
        box_id: { sel: "a.doc-link", type: "exists", cast: "bool" }
      }, allow_empty=false)
    }

xpath_tables_meta = Scraper()
  | extract_meta("meta", include_json_ld=true, index_json_ld=true)
  | $("table.pricing") {
      table(key="pricing_rows", header=true, normalize_headers=true, trim_cells=true)
    }
  | $("table.no-header") {
      table(key="raw_rows", header=false, trim_cells=true)
    }
  | $("ul.xml-items", "xpath_demo") {
      xpath(".//li/text()") -> "xp1_texts"
      | xpath(".//li", attr="data-idx") -> "xp1_attrs"
      | xpath3("string-join(.//li/text(), ', ')") -> "xp3_joined"
      | xpath3(".//li", attr="data-idx") -> "xp3_attrs"
    }
