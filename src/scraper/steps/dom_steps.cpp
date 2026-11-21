#include "scraper/steps/dom_steps.hpp"
#include "scraper/steps/regex_steps.hpp"
#include "scraper/steps/text_steps.hpp"
#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <libxml/HTMLtree.h>
#include <libxml/xmlversion.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace scraper {

namespace {

bool has_non_whitespace(std::string_view s) noexcept {
  return std::any_of(s.begin(), s.end(), [](unsigned char c) {
    return !std::isspace(c);
  });
}

std::string extract_first_attr(const Node &n,
                               const std::vector<std::string> &attrs) {
  for (const auto &attr : attrs) {
    if (auto val = n.Attribute(attr); val && !val->empty()) {
      return *val;
    }
  }
  return {};
}

std::string extract_node_value_opts(const Node &n, QueryAllStep::Type type,
                                    const std::vector<std::string> &attrs,
                                    bool squish, bool trim) {
  std::string out;
  switch (type) {
  case QueryAllStep::Type::Text:
  case QueryAllStep::Type::AdjacentText:
    out = (squish ? n.TextNormalized() : n.Text()).value_or("");
    break;
  case QueryAllStep::Type::OwnText:
    out = (squish ? n.OwnTextNormalized() : n.OwnText()).value_or("");
    break;
  case QueryAllStep::Type::Attribute:
    out = extract_first_attr(n, attrs);
    if (squish && !out.empty()) {
      out = normalizeHtmlText(out);
    }
    break;
  case QueryAllStep::Type::ElementExists:
  case QueryAllStep::Type::AttributeExists:
    return "true";
  }
  if (trim && !squish && !out.empty()) {
    detail::trim_in_place(out);
  }
  return out;
}

} // namespace

namespace detail {

xmlNodePtr find_first_match(xmlNodePtr root, const Selector &sel) {
  if (!root) {
    return nullptr;
  }
  std::vector<xmlNodePtr> stack;
  stack.reserve(64);
  stack.push_back(root);
  while (!stack.empty()) {
    xmlNodePtr cur = stack.back();
    stack.pop_back();
    if (sel.matches(cur)) {
      return cur;
    }
    if (cur->type == XML_ELEMENT_NODE || cur->type == XML_HTML_DOCUMENT_NODE ||
        cur->type == XML_DOCUMENT_NODE) {
      for (xmlNodePtr c = cur->last; c != nullptr; c = c->prev) {
        if (c->type == XML_ELEMENT_NODE) {
          stack.push_back(c);
        }
      }
    }
  }
  return nullptr;
}

} // namespace detail

QueryAllStep::QueryAllStep(std::string_view sel, Type type,
                           std::vector<std::string> attrs, bool squish,
                           bool required)
    : selector_(SelectorParser::Create(std::string(sel))),
      query_type_(type),
      attrs_(std::move(attrs)),
      squish_(squish),
      required_(required) {
  if (!selector_) {
    throw std::runtime_error("find_all: failed to parse CSS selector: " +
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

void QueryAllStep::apply(Node node, std::vector<std::string> &values,
                         jsoncons::json &) {
  if (!node.valid()) {
    if (query_type_ == Type::ElementExists ||
        query_type_ == Type::AttributeExists) {
      values.emplace_back("false");
    } else if (required_) {
      throw std::runtime_error("find_all(" + selector_->toString() +
                               "): invalid node.");
    }
    return;
  }

  if (query_type_ == Type::ElementExists) {
    const bool found =
        detail::find_first_match(node.getRaw(), *selector_) != nullptr;
    values.emplace_back(found ? "true" : "false");
    return;
  }

  QueryResult selected = select_nodes(node);
  if (selected.size() == 0) {
    if (query_type_ == Type::AttributeExists) {
      values.emplace_back("false");
      return;
    }
    if (!required_) {
      return;
    }
    throw std::runtime_error("find_all(" + selector_->toString() +
                             "): no match.");
  }

  bool extracted_any = false;
  for (const auto &n : selected) {
    if (!n.valid()) {
      continue;
    }
    if (query_type_ == Type::AttributeExists) {
      const bool has_attr = std::any_of(
          attrs_.begin(), attrs_.end(),
          [&](const std::string &attr) { return n.HasAttribute(attr); });
      values.emplace_back(has_attr ? "true" : "false");
      extracted_any = true;
      continue;
    }
    std::string val =
        extract_node_value_opts(n, query_type_, attrs_, squish_, false);
    if (!val.empty()) {
      values.push_back(std::move(val));
      extracted_any = true;
    }
  }

  if (required_ && !extracted_any && query_type_ != Type::AttributeExists) {
    throw std::runtime_error("find_all(" + selector_->toString() +
                             "): no match.");
  }
}

FindFirstStep::FindFirstStep(std::string_view sel, QueryAllStep::Type type,
                             std::vector<std::string> attrs, bool squish,
                             bool required)
    : selector_(SelectorParser::Create(std::string(sel))),
      query_type_(type),
      attrs_(std::move(attrs)),
      squish_(squish),
      required_(required) {
  if (!selector_) {
    throw std::runtime_error("find: failed to parse CSS selector: " +
                             std::string(sel));
  }
}

void FindFirstStep::apply(Node node, std::vector<std::string> &values,
                          jsoncons::json &) {
  if (!node.valid()) {
    if (query_type_ == QueryAllStep::Type::ElementExists ||
        query_type_ == QueryAllStep::Type::AttributeExists) {
      values.emplace_back("false");
    } else if (required_) {
      throw std::runtime_error("find(" + selector_->toString() +
                               "): invalid node.");
    }
    return;
  }

  if (query_type_ == QueryAllStep::Type::ElementExists) {
    const bool found =
        detail::find_first_match(node.getRaw(), *selector_) != nullptr;
    values.emplace_back(found ? "true" : "false");
    return;
  }

  if (query_type_ == QueryAllStep::Type::AdjacentText) {
    QueryResult selected = node.NextSiblings().Find(selector_);
    for (const auto &n : selected) {
      if (!n.valid()) {
        continue;
      }
      std::string val =
          extract_node_value_opts(n, query_type_, attrs_, squish_, false);
      if (!val.empty()) {
        values.push_back(std::move(val));
        return;
      }
    }
    if (required_) {
      throw std::runtime_error("find(" + selector_->toString() +
                               "): no match.");
    }
    return;
  }

  // Zero-alloc pre-order traversal stopping on the first match yielding a
  // value (or matching attribute for Attribute/AttributeExists).
  std::vector<xmlNodePtr> stack;
  stack.reserve(64);
  stack.push_back(node.getRaw());
  while (!stack.empty()) {
    xmlNodePtr cur = stack.back();
    stack.pop_back();
    if (selector_->matches(cur)) {
      Node n(cur);
      if (query_type_ == QueryAllStep::Type::AttributeExists) {
        for (const auto &attr : attrs_) {
          if (n.HasAttribute(attr)) {
            values.emplace_back("true");
            return;
          }
        }
      } else {
        std::string val =
            extract_node_value_opts(n, query_type_, attrs_, squish_, false);
        if (!val.empty()) {
          values.push_back(std::move(val));
          return;
        }
      }
    }
    if (cur->type == XML_ELEMENT_NODE || cur->type == XML_HTML_DOCUMENT_NODE ||
        cur->type == XML_DOCUMENT_NODE) {
      for (xmlNodePtr c = cur->last; c != nullptr; c = c->prev) {
        if (c->type == XML_ELEMENT_NODE) {
          stack.push_back(c);
        }
      }
    }
  }

  if (query_type_ == QueryAllStep::Type::AttributeExists) {
    values.emplace_back("false");
  } else if (required_) {
    throw std::runtime_error("find(" + selector_->toString() +
                             "): no match.");
  }
}

ExtractFieldsStep::ExtractFieldsStep(std::vector<FieldSpec> specs,
                                     InsertOptions opts)
    : specs_(std::move(specs)), opts_(opts) {
  for (auto &spec : specs_) {
    spec.selector = SelectorParser::Create(spec.selector_str);
    if (!spec.selector) {
      throw std::runtime_error(
          "extract_fields: failed to parse CSS selector for '" + spec.key +
          "': " + spec.selector_str);
    }
  }
}

void ExtractFieldsStep::apply(Node node, std::vector<std::string> &,
                              jsoncons::json &dict) {
  if (!node.valid() || specs_.empty()) {
    return;
  }

  const size_t n_fields = specs_.size();
  std::vector<std::vector<std::string>> collected(n_fields);
  std::vector<bool> exists_matched(n_fields, false);
  std::vector<size_t> active_tree_fields;
  std::vector<size_t> adjacent_fields;
  active_tree_fields.reserve(n_fields);

  for (size_t i = 0; i < n_fields; ++i) {
    if (specs_[i].type == QueryAllStep::Type::AdjacentText) {
      adjacent_fields.push_back(i);
    } else {
      active_tree_fields.push_back(i);
    }
  }

  if (!active_tree_fields.empty()) {
    std::vector<xmlNodePtr> stack;
    stack.reserve(64);
    stack.push_back(node.getRaw());

    while (!stack.empty() && !active_tree_fields.empty()) {
      xmlNodePtr cur = stack.back();
      stack.pop_back();

      for (size_t pos = 0; pos < active_tree_fields.size();) {
        const size_t idx = active_tree_fields[pos];
        const FieldSpec &spec = specs_[idx];
        bool done = false;

        if (spec.selector->matches(cur)) {
          Node cur_node(cur);
          if (spec.type == QueryAllStep::Type::ElementExists) {
            exists_matched[idx] = true;
            done = true;
          } else if (spec.type == QueryAllStep::Type::AttributeExists) {
            for (const auto &attr : spec.attrs) {
              if (cur_node.HasAttribute(attr)) {
                exists_matched[idx] = true;
                done = true;
                break;
              }
            }
          } else {
            std::string val = extract_node_value_opts(
                cur_node, spec.type, spec.attrs, spec.squish, spec.trim);
            if (has_non_whitespace(val)) {
              collected[idx].push_back(std::move(val));
              if (spec.first_only) {
                done = true;
              }
            }
          }
        }

        if (done) {
          active_tree_fields[pos] = active_tree_fields.back();
          active_tree_fields.pop_back();
        } else {
          ++pos;
        }
      }

      if (cur->type == XML_ELEMENT_NODE ||
          cur->type == XML_HTML_DOCUMENT_NODE ||
          cur->type == XML_DOCUMENT_NODE) {
        for (xmlNodePtr c = cur->last; c != nullptr; c = c->prev) {
          if (c->type == XML_ELEMENT_NODE) {
            stack.push_back(c);
          }
        }
      }
    }
  }

  for (size_t idx : adjacent_fields) {
    const FieldSpec &spec = specs_[idx];
    QueryResult selected = node.NextSiblings().Find(spec.selector);
    for (const auto &m : selected) {
      if (!m.valid()) {
        continue;
      }
      std::string val = extract_node_value_opts(m, spec.type, spec.attrs,
                                                spec.squish, spec.trim);
      if (has_non_whitespace(val)) {
        collected[idx].push_back(std::move(val));
        if (spec.first_only) {
          break;
        }
      }
    }
  }

  for (size_t i = 0; i < n_fields; ++i) {
    const FieldSpec &spec = specs_[i];
    if (spec.type == QueryAllStep::Type::ElementExists ||
        spec.type == QueryAllStep::Type::AttributeExists) {
      jsoncons::json jval =
          spec.cast_explicit
              ? coerce_string_to_json(exists_matched[i] ? "true" : "false",
                                      spec.cast)
              : jsoncons::json(exists_matched[i]);
      insert_into_json(dict, spec.key, std::move(jval), opts_);
      continue;
    }

    auto &vals = collected[i];
    if (vals.empty()) {
      if (spec.default_val.has_value()) {
        insert_into_json(dict, spec.key, jsoncons::json(*spec.default_val),
                         opts_);
      }
      continue;
    }

    if (spec.join_delim.has_value()) {
      std::string joined = absl::StrJoin(vals, *spec.join_delim);
      insert_into_json(dict, spec.key, coerce_string_to_json(joined, spec.cast),
                       opts_);
    } else if (vals.size() == 1) {
      insert_into_json(dict, spec.key,
                       coerce_string_to_json(vals[0], spec.cast), opts_);
    } else {
      jsoncons::json arr = jsoncons::json::array();
      for (const auto &v : vals) {
        arr.push_back(coerce_string_to_json(v, spec.cast));
      }
      insert_into_json(dict, spec.key, std::move(arr), opts_);
    }
  }
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
  if (!node.valid() || attr_names_.empty()) {
    values.emplace_back("");
    return;
  }
  if (attr_names_.size() == 1) {
    values.push_back(node.Attribute(attr_names_[0]).value_or(""));
    return;
  }
  values.push_back(extract_first_attr(node, attr_names_));
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
    insert_into_json(dict, key_name_,
                     coerce_string_to_json(cleaned[0], value_type_), opts_);
  } else if (cleaned.size() > 1) {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &v : cleaned) {
      arr.push_back(coerce_string_to_json(v, value_type_));
    }
    insert_into_json(dict, key_name_, std::move(arr), opts_);
  }
  values.clear();
}

void AppendToStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &dict) {
  jsoncons::json arr = jsoncons::json::array();
  for (auto &v : values) {
    if (allow_empty_ || has_non_whitespace(v)) {
      arr.push_back(std::move(v));
    }
  }
  if (!arr.empty()) {
    InsertOptions opts;
    opts.mode = InsertMode::AppendToList;
    opts.allow_empty = allow_empty_;
    insert_into_json(dict, key_name_, std::move(arr), opts);
  }
  values.clear();
}

void HtmlStep::apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &) {
  if (!node.valid()) {
    return;
  }
  xmlNodePtr raw = node.getRaw();
  std::unique_ptr<xmlBuffer, decltype(&xmlBufferFree)> buf(xmlBufferCreate(),
                                                           xmlBufferFree);
  if (!buf) {
    return;
  }
  if (outer_) {
    htmlNodeDump(buf.get(), raw->doc, raw);
  } else {
    for (xmlNodePtr child = raw->children; child != nullptr;
         child = child->next) {
      htmlNodeDump(buf.get(), raw->doc, child);
    }
  }
  const xmlChar *content = xmlBufferContent(buf.get());
  const int len = xmlBufferLength(buf.get());
  values.emplace_back(
      (content && len > 0)
          ? std::string(reinterpret_cast<const char *>(content),
                        static_cast<size_t>(len))
          : std::string{});
}

void TagNameStep::apply(Node node, std::vector<std::string> &values,
                        jsoncons::json &) {
  if (node.valid()) {
    if (auto tag = node.TagName()) {
      values.push_back(std::move(*tag));
    }
  }
}

void ClassesStep::apply(Node node, std::vector<std::string> &values,
                        jsoncons::json &) {
  if (node.valid()) {
    for (auto &cls : node.Classes()) {
      values.push_back(std::move(cls));
    }
  }
}

void AttrsStep::apply(Node node, std::vector<std::string> &,
                      jsoncons::json &dict) {
  if (!node.valid()) {
    return;
  }
  for (const auto &[k, v] : node.Attributes()) {
    if (data_only_ && !k.starts_with("data-")) {
      continue;
    }
    insert_into_json(dict, k, jsoncons::json(v), opts_);
  }
}

void SortValuesStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  if (values.size() < 2) {
    return;
  }
  if (numeric_) {
    auto parse_num = [](std::string_view s, double &out) -> bool {
      const std::string_view trimmed = absl::StripAsciiWhitespace(s);
      if (trimmed.empty()) {
        return false;
      }
      const auto [ptr, ec] =
          std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), out);
      return ec == std::errc{} && ptr == trimmed.data() + trimmed.size() &&
             std::isfinite(out);
    };
    std::stable_sort(
        values.begin(), values.end(),
        [this, &parse_num](const std::string &a, const std::string &b) {
          double da = 0.0, db = 0.0;
          const bool ok_a = parse_num(a, da);
          const bool ok_b = parse_num(b, db);
          if (ok_a && ok_b) {
            return ascending_ ? (da < db) : (da > db);
          }
          if (ok_a != ok_b) {
            return ok_a;
          }
          return ascending_ ? (a < b) : (a > b);
        });
  } else {
    std::sort(values.begin(), values.end(),
              [this](const std::string &a, const std::string &b) {
                return ascending_ ? (a < b) : (a > b);
              });
  }
}

void ReverseValuesStep::apply(Node, std::vector<std::string> &values,
                              jsoncons::json &) {
  std::reverse(values.begin(), values.end());
}

void CountValuesStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  insert_into_json(
      dict, dict_key_,
      jsoncons::json(static_cast<int64_t>(values.size())), opts_);
  values.clear();
}

SliceSelectionStep::SliceSelectionStep(Mode mode, size_t n, size_t m)
    : mode_(mode), n_param_(n), m_param_(m) {
  if (mode_ == Mode::Range && n_param_ > m_param_) {
    throw std::runtime_error(
        "slice: range start cannot be greater than range end.");
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
  values.erase(std::remove_if(values.begin(), values.end(),
                              [](const std::string &s) {
                                return !has_non_whitespace(s);
                              }),
               values.end());
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

namespace {

struct XmlCharDeleter {
  void operator()(xmlChar *p) const noexcept {
    if (p) {
      xmlFree(p);
    }
  }
};

#if LIBXML_VERSION >= 21200
void silent_xpath_error(void *, const xmlError *) noexcept {}
#else
void silent_xpath_error(void *, xmlErrorPtr) noexcept {}
#endif

std::string xml_char_to_string(xmlChar *ptr) {
  if (!ptr) {
    return {};
  }
  std::unique_ptr<xmlChar, XmlCharDeleter> guard(ptr);
  return std::string(reinterpret_cast<const char *>(guard.get()));
}

std::string trim_ws(std::string_view s) {
  return std::string(absl::StripAsciiWhitespace(s));
}

std::string normalize_header_key(std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  bool prev_underscore = true;
  for (unsigned char c : raw) {
    if (std::isalnum(c)) {
      out.push_back(static_cast<char>(std::tolower(c)));
      prev_underscore = false;
    } else if (!prev_underscore) {
      out.push_back('_');
      prev_underscore = true;
    }
  }
  while (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  return out;
}

int parse_colspan(xmlNodePtr cell) {
  xmlChar *attr = xmlGetProp(cell, reinterpret_cast<const xmlChar *>("colspan"));
  if (!attr) {
    return 1;
  }
  std::string s = xml_char_to_string(attr);
  int val = 1;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
  if (ec != std::errc() || val < 1) {
    return 1;
  }
  return std::min(val, 64);
}

bool tag_equals_ci(xmlNodePtr n, std::string_view target) {
  if (!n || n->type != XML_ELEMENT_NODE || !n->name) {
    return false;
  }
  return absl::EqualsIgnoreCase(reinterpret_cast<const char *>(n->name),
                                target);
}

QueryResult select_css(Node node, const char *css) {
  auto sel = SelectorParser::Create(css);
  if (!sel) {
    return {};
  }
  return sel->MatchAll(node);
}

constexpr int kMaxTableDepth = 64;

void collect_table_rows(xmlNodePtr root,
                        std::vector<std::vector<std::string>> &rows,
                        bool trim_cells, int depth = 0) {
  if (!root || depth > kMaxTableDepth) {
    return;
  }
  for (xmlNodePtr cur = root->children; cur; cur = cur->next) {
    if (cur->type != XML_ELEMENT_NODE) {
      continue;
    }
    if (tag_equals_ci(cur, "thead") || tag_equals_ci(cur, "tbody") ||
        tag_equals_ci(cur, "tfoot")) {
      collect_table_rows(cur, rows, trim_cells, depth + 1);
    } else if (tag_equals_ci(cur, "tr")) {
      std::vector<std::string> row;
      for (xmlNodePtr cell = cur->children; cell; cell = cell->next) {
        if (tag_equals_ci(cell, "th") || tag_equals_ci(cell, "td")) {
          std::string text = xml_char_to_string(xmlNodeGetContent(cell));
          if (trim_cells) {
            text = trim_ws(text);
          }
          const int span = parse_colspan(cell);
          for (int s = 0; s < span; ++s) {
            row.push_back(s == 0 ? text : "");
          }
        }
      }
      if (!row.empty()) {
        rows.push_back(std::move(row));
      }
    }
  }
}

} // namespace

void XPathStep::apply(Node node, std::vector<std::string> &values,
                      jsoncons::json &) {
  if (!node.valid() || expr_.empty()) {
    return;
  }
  xmlNodePtr raw = node.getRaw();
  if (!raw || !raw->doc) {
    return;
  }

  std::unique_ptr<xmlXPathContext, decltype(&xmlXPathFreeContext)> ctx(
      xmlXPathNewContext(raw->doc), &xmlXPathFreeContext);
  if (!ctx) {
    throw std::runtime_error("xpath: failed to create XPath context");
  }
  ctx->node = raw;
  ctx->error = &silent_xpath_error;

  std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)> obj(
      xmlXPathEvalExpression(reinterpret_cast<const xmlChar *>(expr_.c_str()),
                             ctx.get()),
      &xmlXPathFreeObject);
  if (!obj) {
    throw std::runtime_error("xpath: invalid expression '" + expr_ + "'");
  }

  switch (obj->type) {
  case XPATH_NODESET:
    if (obj->nodesetval) {
      for (int i = 0; i < obj->nodesetval->nodeNr; ++i) {
        xmlNodePtr n = obj->nodesetval->nodeTab[i];
        if (!n) {
          continue;
        }
        if (n->type == XML_NAMESPACE_DECL) {
          if (attr_name_.empty()) {
            auto *ns = reinterpret_cast<xmlNsPtr>(n);
            values.emplace_back(
                ns->href ? reinterpret_cast<const char *>(ns->href) : "");
          }
          continue;
        }
        if (!attr_name_.empty()) {
          if (n->type == XML_ELEMENT_NODE) {
            xmlChar *prop = xmlGetProp(
                n, reinterpret_cast<const xmlChar *>(attr_name_.c_str()));
            if (prop) {
              values.push_back(xml_char_to_string(prop));
            }
          }
        } else {
          values.push_back(xml_char_to_string(xmlNodeGetContent(n)));
        }
      }
    }
    break;
  case XPATH_STRING:
    if (obj->stringval) {
      values.emplace_back(reinterpret_cast<const char *>(obj->stringval));
    }
    break;
  case XPATH_NUMBER: {
    std::ostringstream oss;
    oss << obj->floatval;
    values.push_back(oss.str());
    break;
  }
  case XPATH_BOOLEAN:
    values.emplace_back(obj->boolval ? "true" : "false");
    break;
  default:
    break;
  }
}

void TableStep::apply(Node node, std::vector<std::string> &values,
                      jsoncons::json &dict) {
  if (!node.valid()) {
    if (!key_.empty()) {
      values.clear();
    }
    return;
  }

  xmlNodePtr table_raw = node.getRaw();
  if (!tag_equals_ci(table_raw, "table")) {
    auto found = select_css(node, "table");
    if (found.empty() || !found.front()) {
      if (!key_.empty()) {
        values.clear();
      }
      return;
    }
    table_raw = found.front()->getRaw();
  }
  if (!table_raw) {
    if (!key_.empty()) {
      values.clear();
    }
    return;
  }

  std::vector<std::vector<std::string>> rows;
  collect_table_rows(table_raw, rows, trim_cells_);

  jsoncons::json result = jsoncons::json::array();
  if (header_ && !rows.empty()) {
    std::vector<std::string> headers = rows.front();
    absl::flat_hash_set<std::string> seen;
    for (size_t c = 0; c < headers.size(); ++c) {
      std::string h =
          normalize_headers_ ? normalize_header_key(headers[c]) : headers[c];
      if (h.empty()) {
        h = "col_" + std::to_string(c);
      }
      std::string unique_h = h;
      int suffix = 1;
      while (seen.contains(unique_h)) {
        unique_h = h + "_" + std::to_string(++suffix);
      }
      seen.insert(unique_h);
      headers[c] = std::move(unique_h);
    }

    for (size_t r = 1; r < rows.size(); ++r) {
      jsoncons::json obj = jsoncons::json::object();
      for (size_t c = 0; c < headers.size(); ++c) {
        obj.insert_or_assign(headers[c],
                             c < rows[r].size() ? rows[r][c] : std::string{});
      }
      result.push_back(std::move(obj));
    }
  } else {
    for (const auto &row : rows) {
      jsoncons::json arr = jsoncons::json::array();
      for (const auto &cell : row) {
        arr.push_back(cell);
      }
      result.push_back(std::move(arr));
    }
  }

  if (!key_.empty()) {
    insert_into_json(dict, key_, std::move(result), opts_);
    values.clear();
  } else {
    values.push_back(result.to_string());
  }
}

void ExtractMetaStep::apply(Node node, std::vector<std::string> &,
                            jsoncons::json &dict) {
  if (!node.valid()) {
    return;
  }

  jsoncons::json meta = jsoncons::json::object();

  auto titles = select_css(node, "title");
  if (!titles.empty() && titles.front()) {
    if (auto t = titles.front()->Text()) {
      std::string trimmed = trim_ws(*t);
      if (!trimmed.empty()) {
        meta.insert_or_assign("title", std::move(trimmed));
      }
    }
  }

  for (const auto &link_node : select_css(node, "link[rel][href]")) {
    auto rel = link_node.Attribute("rel");
    auto href = link_node.Attribute("href");
    if (rel && href && absl::EqualsIgnoreCase(trim_ws(*rel), "canonical")) {
      meta.insert_or_assign("canonical", trim_ws(*href));
      break;
    }
  }

  for (const auto &m : select_css(node, "meta")) {
    auto content = m.Attribute("content");
    if (!content) {
      continue;
    }
    std::optional<std::string> meta_key = m.Attribute("name");
    if (!meta_key || meta_key->empty()) {
      meta_key = m.Attribute("property");
    }
    if (!meta_key || meta_key->empty()) {
      meta_key = m.Attribute("http-equiv");
    }
    if (!meta_key || meta_key->empty()) {
      continue;
    }
    if (!meta.contains(*meta_key)) {
      meta.insert_or_assign(*meta_key, trim_ws(*content));
    }
  }

  if (include_json_ld_) {
    jsoncons::json ld_array = jsoncons::json::array();
    absl::flat_hash_map<std::string, std::vector<jsoncons::json>> ld_by_type;

    auto index_ld_node = [&](auto &self, const jsoncons::json &item,
                             int depth) -> void {
      if (depth > 32) {
        return;
      }
      if (item.is_array()) {
        for (const auto &el : item.array_range()) {
          self(self, el, depth + 1);
        }
        return;
      }
      if (!item.is_object()) {
        return;
      }
      if (item.contains("@type")) {
        const auto &t = item.at("@type");
        if (t.is_string()) {
          ld_by_type[t.as_string()].push_back(item);
        } else if (t.is_array()) {
          for (const auto &t_el : t.array_range()) {
            if (t_el.is_string()) {
              ld_by_type[t_el.as_string()].push_back(item);
            }
          }
        }
      }
      if (item.contains("@graph")) {
        self(self, item.at("@graph"), depth + 1);
      }
    };

    for (const auto &script :
         select_css(node, "script[type=\"application/ld+json\"]")) {
      if (auto text = script.Text()) {
        std::string trimmed = trim_ws(*text);
        if (trimmed.empty()) {
          continue;
        }
        try {
          jsoncons::json parsed = jsoncons::json::parse(trimmed);
          if (index_json_ld_) {
            index_ld_node(index_ld_node, parsed, 0);
          }
          ld_array.push_back(std::move(parsed));
        } catch (const std::exception &) {
          // Skip malformed JSON-LD blocks gracefully.
        }
      }
    }
    if (!ld_array.empty()) {
      meta.insert_or_assign("json_ld", std::move(ld_array));
    }
    if (index_json_ld_ && !ld_by_type.empty()) {
      jsoncons::json by_type_obj = jsoncons::json::object();
      for (auto &[type_name, items] : ld_by_type) {
        if (items.size() == 1) {
          by_type_obj.insert_or_assign(type_name, std::move(items[0]));
        } else {
          jsoncons::json arr = jsoncons::json::array();
          for (auto &it : items) {
            arr.push_back(std::move(it));
          }
          by_type_obj.insert_or_assign(type_name, std::move(arr));
        }
      }
      meta.insert_or_assign("json_ld_by_type", std::move(by_type_obj));
    }
  }

  if (!key_.empty()) {
    insert_into_json(dict, key_, std::move(meta), opts_);
  } else {
    for (const auto &kv : meta.object_range()) {
      insert_into_json(dict, kv.key(), jsoncons::json(kv.value()), opts_);
    }
  }
}

// ── Phase 2: SubExtractStep / ConditionalStep / GuardStep ────────────────────

void SubExtractStep::apply(Node node, std::vector<std::string> &values,
                           jsoncons::json &dict) {
  ExecutionContext local_ctx;
  apply(node, values, dict, local_ctx);
}

void SubExtractStep::apply(Node node, std::vector<std::string> &,
                           jsoncons::json &dict, ExecutionContext &ctx) {
  if (!node.valid() || !sub_extractor_) {
    return;
  }
  QueryResult sub_sel(node);
  ExtractionOptions opts;
  opts.aborted = &ctx.abort_scraper;
  sub_extractor_->extract(sub_sel, dict, "each", std::cerr, opts);
}

std::shared_ptr<ConditionalStep>
ConditionalStep::make_exists(std::string_view sel, bool negate,
                             std::vector<std::shared_ptr<Step>> steps) {
  SelectorPtr parsed = SelectorParser::Create(std::string(sel));
  if (!parsed) {
    throw std::runtime_error(
        std::string(negate ? "unless_exists" : "when_exists") +
        ": failed to parse CSS selector: " + std::string(sel));
  }
  return std::shared_ptr<ConditionalStep>(
      new ConditionalStep(Kind::Exists, std::move(parsed), nullptr, negate,
                          std::move(steps)));
}

std::shared_ptr<ConditionalStep>
ConditionalStep::make_match(std::string_view pattern, bool negate,
                            const std::optional<std::string> &flags,
                            std::vector<std::shared_ptr<Step>> steps) {
  auto re = std::make_shared<re2::RE2>(std::string(pattern),
                                       detail::get_re2_options(flags));
  if (!re->ok()) {
    throw std::invalid_argument("when_match: invalid regex '" +
                                std::string(pattern) + "': " + re->error());
  }
  return std::shared_ptr<ConditionalStep>(
      new ConditionalStep(Kind::Match, nullptr, std::move(re), negate,
                          std::move(steps)));
}

void ConditionalStep::apply(Node node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  ExecutionContext local_ctx;
  apply(node, values, dict, local_ctx);
}

void ConditionalStep::apply(Node node, std::vector<std::string> &values,
                            jsoncons::json &dict, ExecutionContext &ctx) {
  bool ok = false;
  if (kind_ == Kind::Exists) {
    const bool found =
        node.valid() &&
        detail::find_first_match(node.getRaw(), *selector_) != nullptr;
    ok = negate_ ? !found : found;
  } else {
    bool matched = false;
    for (const auto &v : values) {
      if (re2::RE2::PartialMatch(v, *regex_)) {
        matched = true;
        break;
      }
    }
    ok = negate_ ? !matched : matched;
  }

  if (!ok) {
    return;
  }

  for (const auto &step : steps_) {
    if (!step) {
      continue;
    }
    step->apply(node, values, dict, ctx);
    if (ctx.skip_item || ctx.abort_scraper) {
      break;
    }
  }
}

std::shared_ptr<GuardStep>
GuardStep::make_skip_if_exists(std::string_view sel) {
  SelectorPtr parsed = SelectorParser::Create(std::string(sel));
  if (!parsed) {
    throw std::runtime_error(
        "skip_if_exists: failed to parse CSS selector: " + std::string(sel));
  }
  return std::shared_ptr<GuardStep>(
      new GuardStep(Mode::SkipIfExists, std::move(parsed), "", ""));
}

std::shared_ptr<GuardStep> GuardStep::make_skip_if_empty() {
  return std::shared_ptr<GuardStep>(
      new GuardStep(Mode::SkipIfEmpty, nullptr, "", ""));
}

std::shared_ptr<GuardStep>
GuardStep::make_abort_if_exists(std::string_view sel, std::string_view key,
                                std::string_view reason) {
  SelectorPtr parsed = SelectorParser::Create(std::string(sel));
  if (!parsed) {
    throw std::runtime_error(
        "abort_if_exists: failed to parse CSS selector: " + std::string(sel));
  }
  return std::shared_ptr<GuardStep>(
      new GuardStep(Mode::AbortIfExists, std::move(parsed), key, reason));
}

void GuardStep::apply(Node node, std::vector<std::string> &values,
                      jsoncons::json &dict) {
  ExecutionContext local_ctx;
  apply(node, values, dict, local_ctx);
}

void GuardStep::apply(Node node, std::vector<std::string> &values,
                      jsoncons::json &dict, ExecutionContext &ctx) {
  switch (mode_) {
  case Mode::SkipIfExists:
    if (node.valid() &&
        detail::find_first_match(node.getRaw(), *selector_) != nullptr) {
      ctx.skip_item = true;
    }
    break;
  case Mode::SkipIfEmpty:
    if (std::none_of(values.begin(), values.end(), has_non_whitespace)) {
      ctx.skip_item = true;
    }
    break;
  case Mode::AbortIfExists:
    if (node.valid() &&
        detail::find_first_match(node.getRaw(), *selector_) != nullptr) {
      if (!key_.empty()) {
        jsoncons::json val = reason_.empty() ? jsoncons::json(true)
                                             : jsoncons::json(reason_);
        insert_into_json(dict, key_, std::move(val),
                         InsertOptions{InsertMode::Overwrite, false, true});
      }
      ctx.abort_scraper = true;
    }
    break;
  }
}

} // namespace scraper
