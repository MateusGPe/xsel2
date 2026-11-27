// @playground id=ex_05_regex_fuzzy label="05: Regex, Aho-Corasick & Fuzzy" scraper=regex_ac_fuzzy
regex_ac_fuzzy = Scraper()
  | $("section.regex-box", "regex") {
      find("h1.title", Text) | regex_replace("\\s+", " ") -> "norm"
      | find("h1.title", Text) | regex_extract("#([A-Z]+)-([0-9]+)", 2) -> "order_num"
      | find("h1.title", Text) | regex_groups("(?P<code>[a-z]+)-(?P<num>[0-9]+)", flags="i", prefix="ord_")
      | find("p.prices", Text) | regex_find_all("\\$([0-9]+\\.[0-9]{2})", 1) -> "all_prices"
      | find("pre.multiline", Text, squish=false) | regex_replace("alpha.*BETA", "MATCHED", flags="is") -> "dotall_replaced"
      | find("pre.multiline", Text, squish=false) | split("\n") | regex_filter("^line", negate=false, flags="im") -> "multiline_lines"
      | find_all("ul.codes li", Text) | regex_filter("REFURB", negate=true) -> "non_refurb"
    }
  | $("section.regex-box", "ac") {
      find_all("ul.codes li", Text) | filter(["PRO", "100"]) -> "filtered_codes"
      | find("div.notes", Text) | extract_many(["C++20", "RE2", "Aho-Corasick", "Python"], unique=true) -> "tech_unique"
      | find("div.notes", Text) | extract_many(["C++20", "RE2"], unique=false) -> "tech_all"
    }
  | $("ul.fuzzy-list", "fuzzy") {
      find_all("li", Text) | fuzzy_filter("Mechanical Keyboard", threshold=0.75, algo="jaro_winkler", case_sensitive=false, sort_by_score=true) -> "jw_matches"
      | find_all("li", Text) | fuzzy_filter("Mechanical Keyboard", threshold=0.70, algo="levenshtein", case_sensitive=false, sort_by_score=true) -> "lev_matches"
      | find_all("li", Text) | similarity("Mechanical Keyboard", algo="jaro_winkler", case_sensitive=false, key="jw_scores")
      | find_all("li", Text) | similarity("Mechanical Keyboard", algo="levenshtein", case_sensitive=false) -> "lev_scores_str"
    }
