#pragma once

#include <jsoncons/json.hpp>
#include <string>
#include <string_view>

namespace xsel::common {

inline constexpr int MAX_JSON_DEPTH = 512;

/// Throws `std::runtime_error` if `depth > MAX_JSON_DEPTH`.
void check_json_depth(int depth, std::string_view context);

/// Returns true if `val` is null, an empty string, an empty array, or an empty object.
[[nodiscard]] bool is_empty_json(const jsoncons::json &val) noexcept;

/// Serializes any `jsoncons::json` into a deterministic string key for
/// deduplication and comparison, enforcing `MAX_JSON_DEPTH`.
[[nodiscard]] std::string canonical_json_key(const jsoncons::json &val,
                                             int depth = 0);

/// Lexicographically compares two `jsoncons::json` values using strict total
/// ordering by type rank (null < bool < number < string < array < object).
/// Returns < 0 if a < b, 0 if a == b, > 0 if a > b.
[[nodiscard]] int compare_json(const jsoncons::json &a,
                               const jsoncons::json &b, int depth = 0);

/// Returns `base` if `obj` is not an object or does not contain `base`;
/// otherwise returns `base_0`, `base_1`, ... for the first unused key.
[[nodiscard]] std::string next_unique_key(const jsoncons::json &obj,
                                        std::string_view base);

/// Converts a scalar JSON value to a display string (strings unquoted,
/// numbers/bools/null formatted). Non-scalars are serialized as compact JSON.
[[nodiscard]] std::string json_to_string(const jsoncons::json &val);

} // namespace xsel::common
