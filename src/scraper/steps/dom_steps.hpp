#pragma once

#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <iostream>
#include <lxml2-query.h>
#include <optional>
#include <string>
#include <string_view>

namespace scraper {

class QueryAllStep : public Step {
public:
  enum class Type {
    Text,
    OwnText,
    Attribute,
    ElementExists,
    AttributeExists,
    AdjacentText
  };

  QueryAllStep(std::string_view sel, Type type, std::string_view attr = "");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

protected:
  SelectorPtr selector_;
  Type query_type_;
  std::string attr_name_;

  [[nodiscard]] QueryResult select_nodes(Node node) const;
  [[nodiscard]] bool handle_empty_selection(const QueryResult &selected,
                                            std::vector<std::string> &values,
                                            std::string_view step_name) const;
  [[nodiscard]] std::string
  extract_value_from_node(const std::optional<Node> &node_opt, Type type,
                          std::string_view attr) const;
};

class QueryStep final : public QueryAllStep {
public:
  enum class Number { All, Last, Nth };

  QueryStep(std::string_view sel, Type type, std::string_view attr = "",
            Number number = Number::All, size_t nth_val = 0);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  Number number_;
  size_t nth_element_;
};

class TextStep final : public Step {
public:
  explicit TextStep(bool own_text = false) : use_own_text_(own_text) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool use_own_text_;
};

class AttrStep final : public Step {
public:
  explicit AttrStep(std::string_view attr_name) : attr_name_(attr_name) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string attr_name_;
};

class SetStep final : public Step {
public:
  explicit SetStep(std::string_view name, InsertOptions opts = {})
      : key_name_(name), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string key_name_;
  InsertOptions opts_;
};

class CountValuesStep final : public Step {
public:
  explicit CountValuesStep(std::string_view key, InsertOptions opts = {})
      : dict_key_(key), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string dict_key_;
  InsertOptions opts_;
};

class SliceSelectionStep final : public Step {
public:
  enum class Mode { First, Last, Nth, Range };

  SliceSelectionStep(Mode mode, size_t n = 1, size_t m = 0);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  Mode mode_;
  size_t n_param_;
  size_t m_param_;
};

class UniqueStep final : public Step {
public:
  explicit UniqueStep(bool preserve_order = true)
      : preserve_order_(preserve_order) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool preserve_order_;
};

class DefaultIfEmptyStep final : public Step {
public:
  explicit DefaultIfEmptyStep(std::string_view default_value)
      : default_value_(default_value) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string default_value_;
};

class DebugPrintStep final : public Step {
public:
  DebugPrintStep(std::string_view prefix = "DEBUG: ", bool print_dict = false,
                 bool print_node = false, std::ostream &out = std::cerr)
      : prefix_(prefix), print_dict_(print_dict), print_node_(print_node),
        out_(out) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string prefix_;
  bool print_dict_;
  bool print_node_;
  std::ostream &out_;
};

} // namespace scraper
