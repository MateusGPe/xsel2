#pragma once

#include <absl/container/flat_hash_map.h>
#include <cstddef>
#include <jsoncons/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scraper::dsl {

inline constexpr int MAX_DSL_DEPTH = 256;

struct DslValue {
  struct Identifier {
    std::string name;
  };

  using ObjectType = std::vector<std::pair<std::string, DslValue>>;
  using ValueType =
      std::variant<std::monostate, std::string, int, double, bool,
                   std::vector<DslValue>, ObjectType, Identifier>;

  ValueType value;

  DslValue() : value(std::monostate{}) {}
  explicit DslValue(std::monostate) : value(std::monostate{}) {}
  explicit DslValue(std::string s) : value(std::move(s)) {}
  explicit DslValue(int i) : value(i) {}
  explicit DslValue(double d) : value(d) {}
  explicit DslValue(bool b) : value(b) {}
  explicit DslValue(std::vector<DslValue> l) : value(std::move(l)) {}
  explicit DslValue(ObjectType obj) : value(std::move(obj)) {}
  DslValue(std::string name, bool is_ident);

  [[nodiscard]] bool is_null() const {
    return std::holds_alternative<std::monostate>(value);
  }
  [[nodiscard]] bool is_string() const {
    return std::holds_alternative<std::string>(value);
  }
  [[nodiscard]] bool is_int() const { return std::holds_alternative<int>(value); }
  [[nodiscard]] bool is_double() const {
    return std::holds_alternative<double>(value);
  }
  [[nodiscard]] bool is_bool() const {
    return std::holds_alternative<bool>(value);
  }
  [[nodiscard]] bool is_list() const {
    return std::holds_alternative<std::vector<DslValue>>(value);
  }
  [[nodiscard]] bool is_object() const {
    return std::holds_alternative<ObjectType>(value);
  }
  [[nodiscard]] bool is_identifier() const {
    return std::holds_alternative<Identifier>(value);
  }

  [[nodiscard]] const std::string &get_string() const {
    return std::get<std::string>(value);
  }
  [[nodiscard]] int get_int() const { return std::get<int>(value); }
  [[nodiscard]] double get_double() const { return std::get<double>(value); }
  [[nodiscard]] bool get_bool() const { return std::get<bool>(value); }
  [[nodiscard]] const std::vector<DslValue> &get_list() const {
    return std::get<std::vector<DslValue>>(value);
  }
  [[nodiscard]] const ObjectType &get_object() const {
    return std::get<ObjectType>(value);
  }
  [[nodiscard]] const std::string &get_identifier() const {
    return std::get<Identifier>(value).name;
  }

  [[nodiscard]] std::string to_string() const;
};

struct DslArgList {
  std::vector<DslValue> positional;
  absl::flat_hash_map<std::string, DslValue> named;
};

[[nodiscard]] int parse_strict_int(std::string_view s,
                                   std::string_view context = "DSL");
[[nodiscard]] std::string as_str(const DslValue &v);
[[nodiscard]] bool as_bool(const DslValue &v);
[[nodiscard]] int as_int(const DslValue &v);
[[nodiscard]] size_t as_non_negative_size(const DslValue &v,
                                          std::string_view method);
[[nodiscard]] std::vector<std::string> as_str_list(const DslValue &v);
[[nodiscard]] jsoncons::json dsl_value_to_json(const DslValue &v, int depth = 0);
[[nodiscard]] jsoncons::json as_json_document(const DslValue &v);

[[nodiscard]] const DslValue *find_arg(const DslArgList &args, size_t idx,
                                       std::string_view name);
[[nodiscard]] const DslValue &require_arg(const DslArgList &args, size_t idx,
                                          std::string_view name,
                                          std::string_view method);
[[nodiscard]] std::string opt_str(const DslArgList &args, size_t idx,
                                  std::string_view name,
                                  std::string def = "");
[[nodiscard]] bool opt_bool(const DslArgList &args, size_t idx,
                            std::string_view name, bool def = false);
[[nodiscard]] int opt_int(const DslArgList &args, size_t idx,
                          std::string_view name, int def = 0);
[[nodiscard]] size_t opt_non_negative_size(const DslArgList &args, size_t idx,
                                           std::string_view name,
                                           std::string_view method,
                                           size_t def = 0);

} // namespace scraper::dsl
