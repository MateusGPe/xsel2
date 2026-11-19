#pragma once

#include <jsoncons/json.hpp>
#include <string_view>

namespace xsel::query {

enum class QueryDialect { JmesPath, JsonPath };

/// Evaluates a JMESPath expression against `doc` and returns the transformed
/// JSON result. Throws `std::runtime_error` on invalid syntax or evaluation error.
[[nodiscard]] jsoncons::json evaluate_jmespath(const jsoncons::json &doc,
                                               std::string_view expr);

/// Evaluates a JSONPath query against `doc` and returns the matching JSON
/// array of results. Throws `std::runtime_error` on invalid syntax or error.
[[nodiscard]] jsoncons::json evaluate_jsonpath(const jsoncons::json &doc,
                                               std::string_view expr);

/// Dispatches to `evaluate_jmespath` or `evaluate_jsonpath` by `dialect`.
[[nodiscard]] jsoncons::json evaluate(QueryDialect dialect,
                                      const jsoncons::json &doc,
                                      std::string_view expr);

} // namespace xsel::query
