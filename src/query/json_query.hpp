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

enum class BinaryFormat { Cbor, MsgPack, Bson, Ubjson };
enum class BinaryEncoding { Hex, Base64, Raw };

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

/// Replaces values matching JSONPath `expr` in a copy of `doc` with `replacement`.
[[nodiscard]] jsoncons::json jsonpath_replace(const jsoncons::json &doc,
                                              std::string_view expr,
                                              const jsoncons::json &replacement);

/// Flattens `doc` into an object keyed by normalized JSONPath expressions.
[[nodiscard]] jsoncons::json json_flatten(const jsoncons::json &doc);

/// Unflattens a JSONPath-flattened object back into a nested JSON value.
[[nodiscard]] jsoncons::json json_unflatten(const jsoncons::json &doc);

/// Returns the value at RFC 6901 JSON Pointer `pointer` in `doc`.
[[nodiscard]] jsoncons::json jsonpointer_get(const jsoncons::json &doc,
                                             std::string_view pointer);

/// Inserts or assigns `value` at RFC 6901 JSON Pointer `pointer` in a copy of `doc`.
[[nodiscard]] jsoncons::json jsonpointer_set(const jsoncons::json &doc,
                                             std::string_view pointer,
                                             const jsoncons::json &value);

/// Removes the value at RFC 6901 JSON Pointer `pointer` from a copy of `doc`.
[[nodiscard]] jsoncons::json jsonpointer_remove(const jsoncons::json &doc,
                                                std::string_view pointer);

/// Applies an RFC 6902 JSON Patch array to a copy of `doc`.
[[nodiscard]] jsoncons::json apply_json_patch(const jsoncons::json &doc,
                                              const jsoncons::json &patch);

/// Applies an RFC 7396 JSON Merge Patch to a copy of `doc`.
[[nodiscard]] jsoncons::json apply_merge_patch(const jsoncons::json &doc,
                                               const jsoncons::json &patch);

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

/// Decodes a binary JSON representation (CBOR/MsgPack/BSON/UBJSON) from `input`.
[[nodiscard]] jsoncons::json decode_binary(BinaryFormat fmt,
                                           std::string_view input,
                                           BinaryEncoding enc = BinaryEncoding::Hex);

/// Encodes `doc` into a binary JSON representation (CBOR/MsgPack/BSON/UBJSON).
[[nodiscard]] std::string encode_binary(BinaryFormat fmt,
                                        const jsoncons::json &doc,
                                        BinaryEncoding enc = BinaryEncoding::Hex);

} // namespace xsel::query
