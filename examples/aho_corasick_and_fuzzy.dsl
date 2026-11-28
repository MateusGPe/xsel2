// @playground id=aho_corasick_and_fuzzy label="Aho-Corasick & Fuzzy Matching" html=regex_ac_and_fuzzy.html scraper=ac_and_fuzzy
ac_and_fuzzy = Scraper()
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
