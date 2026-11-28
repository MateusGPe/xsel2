// @playground id=dict_flatten_and_pick label="Dict Flatten & Pick Keys" html=dict_transforms_and_merging.html scraper=flat_demo_scraper
flat_demo_scraper = Scraper()
  | $("section.product", "flat_demo") {
      find("span.a", Text) -> "deep.x"
      | find("span.b", Text) -> "deep.y"
      | flatten()
      | pick_keys(["deep_x"])
    }
