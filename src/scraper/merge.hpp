#pragma once

#include "common/json_utils.hpp"
#include <jsoncons/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

enum class InsertMode {
  Default,      ///< UniqueSuffix for set/count, combine-as-array for merge
  Overwrite,    ///< Replaces existing value at target key
  AppendToList, ///< Combines existing and new values into an array
  UniqueSuffix, ///< Generates key_0, key_1, ... on collision
  Merge         ///< Recursively merges objects by key; combines non-objects
};

struct InsertOptions {
  InsertMode mode = InsertMode::Default;
  bool allow_empty = false;
  bool nested_path = true;
};

[[nodiscard]] std::vector<std::string> split_dict_path(std::string_view key,
                                                       bool nested_path = true);

void insert_into_json(jsoncons::json &dest, std::string_view key,
                      jsoncons::json &&value, const InsertOptions &opts = {});

[[nodiscard]] bool has_no_common_key(const jsoncons::json &obj1,
                                     const jsoncons::json &obj2);

void preprocess_key_value(jsoncons::json &val,
                          InsertMode mode = InsertMode::Default, int depth = 0);

void merge_into(jsoncons::json &dest, const jsoncons::json &other,
                int depth = 0);
void merge_into(jsoncons::json &dest, jsoncons::json &&other, int depth = 0);
void merge_into(jsoncons::json &dest, const jsoncons::json &other,
                InsertMode mode, int depth = 0);
void merge_into(jsoncons::json &dest, jsoncons::json &&other, InsertMode mode,
                int depth = 0);

/// Merges `item` into `dest` at `path_segments` (handling object, null, and
/// array destinations).
void merge_item_at_path(jsoncons::json &dest,
                        const std::vector<std::string> &path_segments,
                        jsoncons::json &&item);

} // namespace scraper
