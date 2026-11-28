// @playground id=regex_extraction_and_filtering label="Regex Extraction & Filtering" html=regex_ac_and_fuzzy.html scraper=regex_extract_filter
regex_extract_filter = Scraper()
  | $("section.regex-box", "regex") {
      find("h1.title", Text) | regex_replace("\\s+", " ") -> "norm"
      | find("h1.title", Text) | regex_extract("#([A-Z]+)-([0-9]+)", 2) -> "order_num"
      | find("h1.title", Text) | regex_groups("(?P<code>[a-z]+)-(?P<num>[0-9]+)", flags="i", prefix="ord_")
      | find("p.prices", Text) | regex_find_all("\\$([0-9]+\\.[0-9]{2})", 1) -> "all_prices"
      | find("pre.multiline", Text, squish=false) | regex_replace("alpha.*BETA", "MATCHED", flags="is") -> "dotall_replaced"
      | find("pre.multiline", Text, squish=false) | split("\n") | regex_filter("^line", negate=false, flags="im") -> "multiline_lines"
      | find_all("ul.codes li", Text) | regex_filter("REFURB", negate=true) -> "non_refurb"
    }
