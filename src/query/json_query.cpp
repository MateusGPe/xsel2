#include "query/json_query.hpp"
#include <jsoncons_ext/jmespath/jmespath.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <stdexcept>
#include <string>

namespace xsel::query {

jsoncons::json evaluate_jmespath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jmespath::search(doc, expr);
  } catch (const jsoncons::jmespath::jmespath_error &e) {
    throw std::runtime_error(std::string("JMESPath error: ") + e.what());
  }
}

jsoncons::json evaluate_jsonpath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jsonpath::json_query(doc, expr);
  } catch (const jsoncons::jsonpath::jsonpath_error &e) {
    throw std::runtime_error(std::string("JSONPath error: ") + e.what());
  }
}

jsoncons::json evaluate(QueryDialect dialect, const jsoncons::json &doc,
                        std::string_view expr) {
  switch (dialect) {
  case QueryDialect::JmesPath:
    return evaluate_jmespath(doc, expr);
  case QueryDialect::JsonPath:
    return evaluate_jsonpath(doc, expr);
  }
  throw std::runtime_error("Unsupported QueryDialect");
}

} // namespace xsel::query
