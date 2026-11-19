#pragma once

#include "digger/token.hpp"
#include "digger/tokenizer.hpp"
#include <jsoncons/json.hpp>
#include <string_view>

namespace digger {

/**
 * Heuristically extracts a typed `jsoncons::json` tree from a JavaScript or
 * JSON-like string_view without executing code.
 *
 * Unquoted scalars are parsed into native JSON types (int64_t, uint64_t,
 * double, bool, null — with JS `undefined` mapped to `null`), while quoted
 * literals remain strings.
 *
 * When `simplify` is true, concatenates `+`-joined strings, unwraps redundant
 * grouping delimiters, strips empty brackets, and recursively expands
 * `JSON.parse("...")` calls before building the output tree.
 */
[[nodiscard]] jsoncons::json parse_object(std::string_view source,
                                          bool simplify = true);

void parse_object_into(std::string_view source, jsoncons::json &out,
                       bool simplify = true);

} // namespace digger
