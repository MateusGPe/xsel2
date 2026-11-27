// @playground id=ex_06_slicing_types label="06: Slicing, Types & Dates" scraper=slicing_and_types
slicing_and_types = Scraper()
  | $("section.numbers", "lists") {
      find_all("ul.scores li", Text) | unique(preserve_order=true) | sort(ascending=true, numeric=true) -> "sorted_num"
      | find_all("ul.scores li", Text) | sort(ascending=false, numeric=false) | reverse() -> "lex_rev"
      | find_all("ul.scores li", Text) | first(2) -> "first_2"
      | find_all("ul.scores li", Text) | last(2) -> "last_2"
      | find_all("ul.scores li", Text) | nth(2) -> "nth_2"
      | find_all("ul.scores li", Text) | slice(1, 2) -> "slice_1_2"
      | find_all("ul.scores li", Text) | unique() | count("unique_count")
      | find_all("ul.scores li", Text) | append_to("collected", allow_empty=false)
      | find("span.int-str", Text) | set("typed_int", type="int")
      | find("span.float-str", Text) | set("typed_float", type="float")
      | find("span.bool-str", Text) | set("typed_bool", type="bool")
    }
  | $("section.numbers", "parsed_nums") {
      find("span.us-price", Text) | parse_number(decimal=".", thousands=",", allow_float=true, key="us")
      | find("span.eu-price", Text) | parse_number(decimal=",", thousands=".", allow_float=true, key="eu")
      | find("span.compact-views", Text) | parse_number(compact=true, key="views")
      | find("span.int-str", Text) | to_int(key="i")
      | find("span.bad-int", Text) | to_int(key="i_fallback", default_val=-1)
      | find("span.float-str", Text) | to_float(key="f", default_val=0.0)
      | find("span.bool-str", Text) | to_bool(key="b", default_val=false)
    }
  | $("section.durations", "durations") {
      find("span.iso-dur", Text) | parse_duration(key="iso_sec", unit="seconds")
      | find("span.human-dur", Text) | parse_duration(key="human_min", unit="minutes")
      | find("span.clock-dur", Text) | parse_duration(key="clock_ms", unit="milliseconds")
    }
  | $("section.dates", "dates") {
      find("time.iso", Attribute, "datetime") | parse_date(output="iso", tz="UTC", key="iso")
      | find("time.iso", Attribute, "datetime") | parse_date(output="date", tz="UTC", key="date_only")
      | find("time.iso", Attribute, "datetime") | parse_date(output="unix", tz="UTC", key="unix")
      | find("time.iso", Attribute, "datetime") | parse_date(output="unix_ms", tz="UTC", key="unix_ms")
      | find("span.custom", Text) | parse_date(fmt="%Y/%m/%d %H:%M:%S", output="%Y-%m-%d", tz="UTC", key="custom")
      | find("span.relative", Text) | parse_date(output="date", tz="UTC", now="2026-05-10T00:00:00Z", key="rel_date")
    }
  | $("div.empty-fallback", "fallback") {
      text | default("N/A") -> "val"
    }
