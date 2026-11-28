// @playground id=dict_transforms label="Dict Transforms & Merging" html=dict_transforms_and_merging.html scraper=dict_ops
dict_ops = Scraper()
  | $("section.product", "item") {
      @"data-sku" -> "sku"
      | find("h1.title", Text) -> "title"
      | find("span.alt-title", Text) -> "fallback_title"
      | find("span.price-str", Text) -> "price"
      | find("span.qty-str", Text) -> "qty"
      | find("span.a", Text) -> "nested.a"
      | find("span.b", Text) -> "nested.b"
      | find_all("ul.tags li", Text) -> "tags"
      | find_all("dl.specs dt", Text) -> "_k"
      | find_all("dl.specs dd", Text) -> "_v"
      | zip_keys("_k", "_v", "specs")
      | omit_keys(["_k", "_v"])
      | cast_keys({"price": "float", "qty": "int"})
      | coalesce_keys({"headline": ["missing_title", "fallback_title", "title"]}, drop_sources=true)
      | rename_keys({"sku": "product_sku"})
      | merge_keys(["product_sku", "headline"], "summary_parts")
      | deduplicate()
      | sort_array(ascending=true)
      | debug(prefix="DICT_DSL: ", print_dict=false, print_node=false)
    }
