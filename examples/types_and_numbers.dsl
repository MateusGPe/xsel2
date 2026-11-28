// @playground id=types_and_numbers label="Type Casting & Number Parsing" html=slicing_ordering_and_types.html scraper=types_and_numbers
types_and_numbers = Scraper()
  | $("section.numbers", "lists") {
      find("span.int-str", Text) | set("typed_int", type="int")
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
