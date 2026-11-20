#pragma once

#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <absl/container/flat_hash_map.h>
#include <jsoncons/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

enum class ValueType { String, Int, Float, Number, Bool, Auto };

[[nodiscard]] ValueType parse_value_type_name(std::string_view type_name);

/// Coerces a raw string into a typed `jsoncons::json` according to `type`.
[[nodiscard]] jsoncons::json coerce_string_to_json(std::string_view s,
                                                   ValueType type);

/// Normalizes a messy numeric string (e.g., "$1,299.90", "(42.5)") into a
/// canonical ASCII number string, or returns std::nullopt if no number found.
[[nodiscard]] std::optional<std::string>
extract_number_string(std::string_view input, std::string_view decimal = ".",
                      std::string_view thousands = ",",
                      bool allow_float = true);

class ParseNumberStep final : public Step {
public:
  ParseNumberStep(std::string_view decimal = ".",
                  std::string_view thousands = ",", bool allow_float = true,
                  std::string_view key = "", InsertOptions opts = {})
      : decimal_(decimal), thousands_(thousands), allow_float_(allow_float),
        key_(key), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string decimal_;
  std::string thousands_;
  bool allow_float_;
  std::string key_;
  InsertOptions opts_;
};

class CastValuesStep final : public Step {
public:
  CastValuesStep(ValueType target_type, std::string_view key = "",
                 std::optional<jsoncons::json> default_val = std::nullopt,
                 InsertOptions opts = {})
      : target_type_(target_type), key_(key),
        default_val_(std::move(default_val)), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  ValueType target_type_;
  std::string key_;
  std::optional<jsoncons::json> default_val_;
  InsertOptions opts_;
};

class CastKeysStep final : public PostponableJsonStep {
public:
  CastKeysStep(absl::flat_hash_map<std::string, ValueType> schema,
               bool postponable = true)
      : PostponableJsonStep(postponable), schema_(std::move(schema)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_map<std::string, ValueType> schema_;
  [[nodiscard]] jsoncons::json cast_impl(const jsoncons::json &input,
                                         int depth) const;
};

} // namespace scraper
