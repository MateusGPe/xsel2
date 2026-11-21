#pragma once

#include <jsoncons/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace xsel::query {

enum class QueryDialect { JmesPath, JsonPath };

struct JsonParseOptions {
  bool allow_comments = false;
  bool allow_trailing_comma = false;
  bool allow_inf_and_nan = false;
  bool lossless_number = false;
  int max_nesting_depth = 1024;
};

struct CsvOptions {
  bool assume_header = true;
  char delimiter = ',';
  bool trim = true;
};

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

/// Parses a JSON string using `jsoncons::json::parse` with `opts`.
[[nodiscard]] jsoncons::json parse_json(std::string_view input,
                                        const JsonParseOptions &opts = {});

/// Serializes `doc` to a JSON string (compact or pretty-printed).
[[nodiscard]] std::string dump_json(const jsoncons::json &doc,
                                    bool pretty = false, int indent = 2);

/// Validates `doc` against `schema`. Returns true if valid; collects
/// formatted error strings into `errors_out` when provided.
[[nodiscard]] bool validate_json_schema(const jsoncons::json &doc,
                                        const jsoncons::json &schema,
                                        std::vector<std::string> *errors_out = nullptr);

/// Decodes a CSV string into a JSON array (objects when `assume_header=true`).
[[nodiscard]] jsoncons::json decode_csv(std::string_view input,
                                        const CsvOptions &opts = {});

/// Encodes a JSON array/value into a CSV string.
[[nodiscard]] std::string encode_csv(const jsoncons::json &doc,
                                     const CsvOptions &opts = {});

} // namespace xsel::query
