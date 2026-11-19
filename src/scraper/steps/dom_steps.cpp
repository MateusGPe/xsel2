#include "scraper/steps/dom_steps.hpp"
#include <absl/container/flat_hash_set.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace scraper {

namespace {

bool has_non_whitespace(std::string_view s) noexcept {
  return std::any_of(s.begin(), s.end(), [](unsigned char c) {
    return !std::isspace(c);
  });
}

} // namespace

QueryAllStep::QueryAllStep(std::string_view sel, Type type,
                           std::string_view attr)
    : selector_(SelectorParser::Create(std::string(sel))),
      query_type_(type),
      attr_name_(attr) {
  if (!selector_) {
    throw std::runtime_error("QueryAll: failed to parse CSS selector: " +
                             std::string(sel));
  }
}

QueryResult QueryAllStep::select_nodes(Node node) const {
  if (!node.valid()) {
    return QueryResult(QueryResult::NodeSet{});
  }
  return (query_type_ == Type::AdjacentText)
             ? node.NextSiblings().Find(selector_)
             : QueryResult(node).Find(selector_);
}

std::string
QueryAllStep::extract_value_from_node(const std::optional<Node> &node_opt,
                                      Type type, std::string_view attr) const {
  const Node &current_node = node_opt.value_or(Node(nullptr));
  switch (type) {
  case Type::Text:
  case Type::AdjacentText:
    return current_node.Text().value_or("");
  case Type::OwnText:
    return current_node.OwnText().value_or("");
  case Type::Attribute:
    return current_node.Attribute(std::string(attr)).value_or("");
  case Type::ElementExists:
    return node_opt.has_value() ? "true" : "false";
  case Type::AttributeExists:
    return (node_opt.has_value() &&
            current_node.Attribute(std::string(attr)).has_value())
               ? "true"
               : "false";
  }
  return "";
}

void QueryAllStep::apply(Node node, std::vector<std::string> &values,
                         jsoncons::json &) {
  QueryResult selected = select_nodes(node);

  if (selected.size() == 0 && query_type_ != Type::ElementExists &&
      query_type_ != Type::AttributeExists) {
    std::ostringstream err;
    err << "queryAll(" << std::quoted(selector_->toString())
        << "): no match for non-existence query type.";
    throw std::runtime_error(err.str());
  }

  for (const auto &n : selected) {
    if (!n.valid() && query_type_ != Type::ElementExists &&
        query_type_ != Type::AttributeExists) {
      continue;
    }
    std::string val = extract_value_from_node(n, query_type_, attr_name_);
    if (!val.empty() || query_type_ == Type::ElementExists ||
        query_type_ == Type::AttributeExists) {
      values.push_back(std::move(val));
    }
  }
}

QueryStep::QueryStep(std::string_view sel, Type type, std::string_view attr,
                     Number number, size_t nth_val)
    : QueryAllStep(sel, type, attr),
      number_(number),
      nth_element_(number == Number::Last ? static_cast<size_t>(-1) : nth_val) {
}

void QueryStep::apply(Node node, std::vector<std::string> &values,
                      jsoncons::json &dict) {
  if (number_ == Number::All) {
    QueryAllStep::apply(node, values, dict);
    return;
  }

  QueryResult selected = select_nodes(node);
  if (selected.size() == 0 && query_type_ != Type::ElementExists &&
      query_type_ != Type::AttributeExists) {
    std::ostringstream err;
    err << "query(" << std::quoted(selector_->toString())
        << "): no match for non-existence query type.";
    throw std::runtime_error(err.str());
  }

  if (selected.empty()) {
    if (query_type_ == Type::ElementExists ||
        query_type_ == Type::AttributeExists) {
      values.push_back(
          extract_value_from_node(std::nullopt, query_type_, attr_name_));
    }
    return;
  }

  size_t idx = nth_element_;
  if (idx >= selected.size()) {
    idx = selected.size() - 1;
  }
  values.push_back(
      extract_value_from_node(selected[idx], query_type_, attr_name_));
}

void TextStep::apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &) {
  if (node.valid()) {
    values.push_back(
        (use_own_text_ ? node.OwnText() : node.Text()).value_or(""));
  } else {
    values.emplace_back("");
  }
}

void AttrStep::apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &) {
  if (node.valid()) {
    values.push_back(node.Attribute(attr_name_).value_or(""));
  } else {
    values.emplace_back("");
  }
}

void SetStep::apply(Node, std::vector<std::string> &values,
                    jsoncons::json &dict) {
  std::vector<std::string> cleaned;
  cleaned.reserve(values.size());
  for (auto &v : values) {
    if (opts_.allow_empty || has_non_whitespace(v)) {
      cleaned.push_back(std::move(v));
    }
  }

  if (cleaned.size() == 1) {
    insert_into_json(dict, key_name_, jsoncons::json(std::move(cleaned[0])),
                     opts_);
  } else if (cleaned.size() > 1) {
    jsoncons::json arr = jsoncons::json::array();
    for (auto &v : cleaned) {
      arr.push_back(std::move(v));
    }
    insert_into_json(dict, key_name_, std::move(arr), opts_);
  }
  values.clear();
}

void CountValuesStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  insert_into_json(
      dict, dict_key_,
      jsoncons::json(static_cast<int64_t>(values.size())), opts_);
}

SliceSelectionStep::SliceSelectionStep(Mode mode, size_t n, size_t m)
    : mode_(mode), n_param_(n), m_param_(m) {
  if (mode_ == Mode::Range && n_param_ > m_param_) {
    throw std::runtime_error(
        "slice_values: range start cannot be greater than range end.");
  }
}

void SliceSelectionStep::apply(Node, std::vector<std::string> &values,
                               jsoncons::json &) {
  if (values.empty())
    return;

  std::vector<std::string> out;
  const size_t sz = values.size();
  switch (mode_) {
  case Mode::First:
    out.reserve(std::min(n_param_, sz));
    for (size_t i = 0; i < n_param_ && i < sz; ++i) {
      out.push_back(std::move(values[i]));
    }
    break;
  case Mode::Last:
    out.reserve(std::min(n_param_, sz));
    for (size_t i = 0; i < n_param_ && i < sz; ++i) {
      out.push_back(std::move(values[sz - 1 - i]));
    }
    std::reverse(out.begin(), out.end());
    break;
  case Mode::Nth:
    if (n_param_ < sz) {
      out.push_back(std::move(values[n_param_]));
    }
    break;
  case Mode::Range:
    if (n_param_ < sz) {
      const size_t end_idx = std::min(m_param_, sz - 1);
      out.reserve(end_idx >= n_param_ ? end_idx - n_param_ + 1 : 0);
      for (size_t i = n_param_; i <= end_idx; ++i) {
        out.push_back(std::move(values[i]));
      }
    }
    break;
  }
  values = std::move(out);
}

void UniqueStep::apply(Node, std::vector<std::string> &values,
                       jsoncons::json &) {
  if (values.empty())
    return;
  if (preserve_order_) {
    std::vector<std::string> out;
    out.reserve(values.size());
    absl::flat_hash_set<std::string> seen;
    seen.reserve(values.size());
    for (auto &v : values) {
      if (seen.insert(v).second) {
        out.push_back(std::move(v));
      }
    }
    values = std::move(out);
  } else {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
  }
}

void DefaultIfEmptyStep::apply(Node, std::vector<std::string> &values,
                               jsoncons::json &) {
  if (values.empty()) {
    values.push_back(default_value_);
  }
}

void DebugPrintStep::apply(Node node, std::vector<std::string> &values,
                           jsoncons::json &dict) {
  out_ << "--- " << prefix_ << " ---\n";
  out_ << "Values (" << values.size() << "):\n";
  for (size_t i = 0; i < values.size(); ++i) {
    out_ << "  [" << i << "]: " << std::quoted(values[i]) << '\n';
  }
  if (print_dict_) {
    out_ << "Dict:\n" << jsoncons::pretty_print(dict) << '\n';
  }
  if (print_node_ && node.valid()) {
    out_ << "Current Node Text: "
         << std::quoted(node.Text().value_or("<no text>")) << '\n';
    out_ << "Current Node OwnText: "
         << std::quoted(node.OwnText().value_or("<no own text>")) << '\n';
  }
  out_ << "--- END " << prefix_ << " ---" << std::endl;
}

} // namespace scraper
