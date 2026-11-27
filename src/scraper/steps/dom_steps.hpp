#pragma once

#include "scraper/extractor.hpp"
#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include "scraper/steps/type_steps.hpp"
#include <concepts>
#include <iostream>
#include <lxml2-query.h>
#include <memory>
#include <optional>
#include <re2/re2.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scraper {

namespace detail {
[[nodiscard]] xmlNodePtr find_first_match(xmlNodePtr root, const Selector &sel);
} // namespace detail

class QueryAllStep final : public Step {
public:
  enum class Type {
    Text,
    OwnText,
    Attribute,
    ElementExists,
    AttributeExists,
    AdjacentText
  };

  QueryAllStep(std::string_view sel, Type type = Type::Text,
               std::vector<std::string> attrs = {}, bool squish = false,
               bool required = false);

  template <std::convertible_to<std::string_view> AttrStr>
  QueryAllStep(std::string_view sel, Type type, AttrStr &&attr,
               bool squish = false, bool required = false)
      : QueryAllStep(
            sel, type,
            make_attr_vec(std::string_view(std::forward<AttrStr>(attr))),
            squish, required) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  static std::vector<std::string> make_attr_vec(std::string_view attr) {
    if (attr.empty()) {
      return {};
    }
    return {std::string(attr)};
  }

  SelectorPtr selector_;
  Type query_type_;
  std::vector<std::string> attrs_;
  bool squish_;
  bool required_;

  [[nodiscard]] QueryResult select_nodes(Node node) const;
};

class FindFirstStep final : public Step {
public:
  FindFirstStep(std::string_view sel,
                QueryAllStep::Type type = QueryAllStep::Type::Text,
                std::vector<std::string> attrs = {}, bool squish = true,
                bool required = false);

  template <std::convertible_to<std::string_view> AttrStr>
  FindFirstStep(std::string_view sel, QueryAllStep::Type type, AttrStr &&attr,
                bool squish = true, bool required = false)
      : FindFirstStep(
            sel, type,
            make_attr_vec(std::string_view(std::forward<AttrStr>(attr))),
            squish, required) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  static std::vector<std::string> make_attr_vec(std::string_view attr) {
    if (attr.empty()) {
      return {};
    }
    return {std::string(attr)};
  }

  SelectorPtr selector_;
  QueryAllStep::Type query_type_;
  std::vector<std::string> attrs_;
  bool squish_;
  bool required_;
};

struct FieldSpec {
  std::string key;
  std::string selector_str;
  SelectorPtr selector;
  QueryAllStep::Type type = QueryAllStep::Type::Text;
  std::vector<std::string> attrs;
  bool squish = true;
  bool trim = false;
  bool first_only = true;
  ValueType cast = ValueType::String;
  bool cast_explicit = false;
  std::optional<jsoncons::json> default_val;
  std::optional<std::string> join_delim;
};

class ExtractFieldsStep final : public Step {
public:
  explicit ExtractFieldsStep(std::vector<FieldSpec> specs,
                             InsertOptions opts = {});

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<FieldSpec> specs_;
  InsertOptions opts_;
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
  explicit AttrStep(std::string_view attr_name) {
    attr_names_.emplace_back(attr_name);
  }
  explicit AttrStep(std::vector<std::string> attr_names)
      : attr_names_(std::move(attr_names)) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<std::string> attr_names_;
};

class SetStep final : public Step {
public:
  explicit SetStep(std::string_view name, InsertOptions opts = {},
                   ValueType value_type = ValueType::String)
      : key_name_(name), opts_(opts), value_type_(value_type) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string key_name_;
  InsertOptions opts_;
  ValueType value_type_;
};

class AppendToStep final : public Step {
public:
  explicit AppendToStep(std::string_view key, bool allow_empty = false)
      : key_name_(key), allow_empty_(allow_empty) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string key_name_;
  bool allow_empty_;
};

class HtmlStep final : public Step {
public:
  explicit HtmlStep(bool outer = false) : outer_(outer) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool outer_;
};

class TagNameStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class ClassesStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class AttrsStep final : public Step {
public:
  explicit AttrsStep(bool data_only = false, InsertOptions opts = {})
      : data_only_(data_only), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool data_only_;
  InsertOptions opts_;
};

class SortValuesStep final : public Step {
public:
  SortValuesStep(bool ascending = true, bool numeric = false)
      : ascending_(ascending), numeric_(numeric) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool ascending_;
  bool numeric_;
};

class ReverseValuesStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
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

class XPathStep final : public Step {
public:
  explicit XPathStep(std::string_view expr, std::string_view attr = "")
      : expr_(expr), attr_name_(attr) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string expr_;
  std::string attr_name_;
};

#ifdef XSEL_USE_XPATH3
/// Evaluates an XPath 3.1 expression via lxml2-query and appends extracted
/// node/atomic/array string values (or attribute values when attr is non-empty).
class XPath3Step final : public Step {
public:
  explicit XPath3Step(std::string_view expr, std::string_view attr = "");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string expr_;
  std::string attr_name_;
  lxml2::xpath::XPathExprPtr compiled_expr_;
};
#endif

class TableStep final : public Step {
public:
  TableStep(std::string_view key = "", bool header = true,
            bool normalize_headers = false, bool trim_cells = true,
            InsertOptions opts = {})
      : key_(key), header_(header), normalize_headers_(normalize_headers),
        trim_cells_(trim_cells), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string key_;
  bool header_;
  bool normalize_headers_;
  bool trim_cells_;
  InsertOptions opts_;
};

class ExtractMetaStep final : public Step {
public:
  ExtractMetaStep(std::string_view key = "meta", bool include_json_ld = true,
                  InsertOptions opts = {}, bool index_json_ld = false)
      : key_(key), include_json_ld_(include_json_ld), opts_(opts),
        index_json_ld_(index_json_ld) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string key_;
  bool include_json_ld_;
  InsertOptions opts_;
  bool index_json_ld_;
};

class SubExtractStep final : public Step {
public:
  explicit SubExtractStep(std::shared_ptr<Extractor> sub_extractor)
      : sub_extractor_(std::move(sub_extractor)) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  std::shared_ptr<Extractor> sub_extractor_;
};

class ConditionalStep final : public Step {
public:
  static std::shared_ptr<ConditionalStep>
  make_exists(std::string_view sel, bool negate,
              std::vector<std::shared_ptr<Step>> steps);

  static std::shared_ptr<ConditionalStep>
  make_match(std::string_view pattern, bool negate,
             const std::optional<std::string> &flags,
             std::vector<std::shared_ptr<Step>> steps);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  enum class Kind { Exists, Match };

  Kind kind_;
  SelectorPtr selector_;
  std::shared_ptr<re2::RE2> regex_;
  bool negate_;
  std::vector<std::shared_ptr<Step>> steps_;

  ConditionalStep(Kind kind, SelectorPtr selector,
                  std::shared_ptr<re2::RE2> regex, bool negate,
                  std::vector<std::shared_ptr<Step>> steps)
      : kind_(kind), selector_(std::move(selector)), regex_(std::move(regex)),
        negate_(negate), steps_(std::move(steps)) {}
};

class GuardStep final : public Step {
public:
  enum class Mode { SkipIfExists, SkipIfEmpty, AbortIfExists };

  static std::shared_ptr<GuardStep> make_skip_if_exists(std::string_view sel);
  static std::shared_ptr<GuardStep> make_skip_if_empty();
  static std::shared_ptr<GuardStep>
  make_abort_if_exists(std::string_view sel, std::string_view key = "",
                       std::string_view reason = "");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  Mode mode_;
  SelectorPtr selector_;
  std::string key_;
  std::string reason_;

  GuardStep(Mode mode, SelectorPtr selector, std::string_view key,
            std::string_view reason)
      : mode_(mode), selector_(std::move(selector)), key_(key),
        reason_(reason) {}
};

} // namespace scraper
