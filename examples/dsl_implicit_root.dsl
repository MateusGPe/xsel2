// @playground id=dsl_implicit_root label="DSL Implicit Root & Key-Value" html=dsl_syntax_and_composition.html scraper=implicit_root
// Implicit root scraper + @key/@value collapsing
implicit_root = .select("dl.specs") {
  find("dt", Text) -> "@key"
  | find("dd", Text) -> "@value"
}
