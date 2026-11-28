// @playground id=slicing_and_ordering label="List Slicing, Ordering & Defaults" html=slicing_ordering_and_types.html scraper=slicing_and_ordering
slicing_and_ordering = Scraper()
  | $("section.numbers", "lists") {
      find_all("ul.scores li", Text) | unique(preserve_order=true) | sort(ascending=true, numeric=true) -> "sorted_num"
      | find_all("ul.scores li", Text) | sort(ascending=false, numeric=false) | reverse() -> "lex_rev"
      | find_all("ul.scores li", Text) | first(2) -> "first_2"
      | find_all("ul.scores li", Text) | last(2) -> "last_2"
      | find_all("ul.scores li", Text) | nth(2) -> "nth_2"
      | find_all("ul.scores li", Text) | slice(1, 2) -> "slice_1_2"
      | find_all("ul.scores li", Text) | unique() | count("unique_count")
      | find_all("ul.scores li", Text) | append_to("collected", allow_empty=false)
    }
  | $("div.empty-fallback", "fallback") {
      text | default("N/A") -> "val"
    }
