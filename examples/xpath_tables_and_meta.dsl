// @playground id=xpath_tables_and_meta label="XPath, Tables & Metadata" html=dom_and_selection.html scraper=xpath_tables_meta
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
