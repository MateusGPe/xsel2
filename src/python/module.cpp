#include "common/ahocorasick.hpp"
#include "digger/digger.hpp"
#include "python/json_py_bridge.hpp"
#include "query/json_query.hpp"
#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <absl/strings/match.h>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string_view>

namespace py = pybind11;

namespace {

class DocumentPy {
public:
  Document doc;

  static std::shared_ptr<DocumentPy> from_string(const std::string &html) {
    auto d = std::make_shared<DocumentPy>();
    d->doc.parseMemory(html);
    return d;
  }

  static std::shared_ptr<DocumentPy> from_file(const std::string &path) {
    auto d = std::make_shared<DocumentPy>();
    if (!d->doc.parse(path)) {
      throw std::runtime_error("Failed to open or parse HTML file: " + path);
    }
    return d;
  }
};

class ScraperPy final : public scraper::ScraperBuilderBase<ScraperPy> {
public:
  using scraper::ScraperBuilderBase<ScraperPy>::ScraperBuilderBase;

  py::object from_string_py(const std::string &content) {
    return xsel::python::json_to_python(scrape_from_string(content));
  }

  py::object from_file_py(const std::string &filename) {
    return xsel::python::json_to_python(scrape_from_file(filename));
  }

  py::object from_document_py(DocumentPy &doc_py) {
    jsoncons::json result = jsoncons::json::object();
    if (doc_py.doc.isValid()) {
      scrape_document(doc_py.doc, result, "document");
    }
    return xsel::python::json_to_python(result);
  }
};

std::vector<std::string> py_str_or_list(const py::object &obj) {
  if (obj.is_none()) {
    return {};
  }
  if (py::isinstance<py::str>(obj)) {
    return {obj.cast<std::string>()};
  }
  return obj.cast<std::vector<std::string>>();
}

scraper::QueryAllStep::Type py_parse_value_type(const py::object &obj) {
  if (py::isinstance<scraper::QueryAllStep::Type>(obj)) {
    return obj.cast<scraper::QueryAllStep::Type>();
  }
  const std::string s = obj.cast<std::string>();
  if (s == "Value.Text" || s == "Text" || s == "text") {
    return scraper::QueryAllStep::Type::Text;
  }
  if (s == "Value.OwnText" || s == "OwnText" || s == "own_text") {
    return scraper::QueryAllStep::Type::OwnText;
  }
  if (s == "Value.Attribute" || s == "Attribute" || s == "attr") {
    return scraper::QueryAllStep::Type::Attribute;
  }
  if (s == "Value.ElementExists" || s == "ElementExists" || s == "exists") {
    return scraper::QueryAllStep::Type::ElementExists;
  }
  if (s == "Value.AttributeExists" || s == "AttributeExists" ||
      s == "attr_exists") {
    return scraper::QueryAllStep::Type::AttributeExists;
  }
  if (s == "Value.Adjacent" || s == "Adjacent" || s == "adjacent") {
    return scraper::QueryAllStep::Type::AdjacentText;
  }
  throw std::runtime_error("Unknown Value type: " + s);
}

ScraperPy &py_apply_extract_fields(ScraperPy &self, const py::dict &spec,
                                   bool allow_empty) {
  std::vector<scraper::FieldSpec> specs;
  specs.reserve(spec.size());
  for (const auto &item : spec) {
    scraper::FieldSpec fs;
    fs.key = item.first.cast<std::string>();
    if (!py::isinstance<py::dict>(item.second)) {
      fs.selector_str = item.second.cast<std::string>();
      specs.push_back(std::move(fs));
      continue;
    }
    py::dict d = item.second.cast<py::dict>();
    bool has_sel = false;
    bool has_type = false;
    bool has_attr = false;
    for (const auto &kv : d) {
      const std::string fk = kv.first.cast<std::string>();
      py::object fv = py::reinterpret_borrow<py::object>(kv.second);
      if (fk == "sel" || fk == "selector") {
        fs.selector_str = fv.cast<std::string>();
        has_sel = true;
      } else if (fk == "type") {
        fs.type = py_parse_value_type(fv);
        has_type = true;
      } else if (fk == "attr") {
        if (!fv.is_none()) {
          fs.attrs = py_str_or_list(fv);
          has_attr = true;
        }
      } else if (fk == "squish") {
        fs.squish = fv.cast<bool>();
      } else if (fk == "trim") {
        fs.trim = fv.cast<bool>();
      } else if (fk == "first") {
        fs.first_only = fv.cast<bool>();
      } else if (fk == "cast" || fk == "as" || fk == "to") {
        fs.cast = scraper::parse_value_type_name(fv.cast<std::string>());
        fs.cast_explicit = true;
      } else if (fk == "default") {
        fs.default_val = xsel::python::python_to_json(fv);
      } else if (fk == "join") {
        if (!fv.is_none()) {
          fs.join_delim = fv.cast<std::string>();
        }
      } else {
        throw std::runtime_error("extract_fields: unknown field option '" + fk +
                                 "' for '" + fs.key + "'");
      }
    }
    if (!has_sel) {
      throw std::runtime_error(
          "extract_fields: missing 'sel' in field spec for '" + fs.key + "'");
    }
    if (!has_type && has_attr) {
      fs.type = scraper::QueryAllStep::Type::Attribute;
    }
    specs.push_back(std::move(fs));
  }
  scraper::InsertOptions opts;
  opts.allow_empty = allow_empty;
  return self.ExtractFields(std::move(specs), opts);
}

ScraperPy &py_apply_js_blocks(ScraperPy &self, const py::object &prefixes,
                              const py::object &aliases) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (py::isinstance<py::dict>(prefixes)) {
    for (const auto &item : prefixes.cast<py::dict>()) {
      entries.emplace_back(item.first.cast<std::string>(),
                           item.second.cast<std::string>());
    }
  } else {
    for (const auto &p : py_str_or_list(prefixes)) {
      if (p.size() > 1 && p.back() == '*') {
        std::string prefix = p.substr(0, p.size() - 1);
        std::string key = prefix;
        if (key.size() > 1 && key.back() == '_') {
          key.pop_back();
        }
        entries.emplace_back(std::move(prefix), std::move(key));
      } else {
        entries.emplace_back(p, p);
      }
    }
  }
  if (!aliases.is_none()) {
    for (const auto &item : aliases.cast<py::dict>()) {
      entries.emplace_back(item.first.cast<std::string>(),
                           item.second.cast<std::string>());
    }
  }
  return self.JsBlocksAliased(std::move(entries));
}

py::dict scrapers_to_py_dict(
    absl::flat_hash_map<std::string, scraper::ScraperBuilder> &parsed) {
  py::dict out;
  for (auto &[name, builder] : parsed) {
    ScraperPy py_scraper;
    py_scraper.set_extractors(builder.get_extractors());
    py_scraper.set_finalize_steps(builder.get_finalize_steps());
    out[py::str(name)] = py::cast(std::move(py_scraper));
  }
  return out;
}

xsel::query::BinaryEncoding
parse_py_binary_encoding(std::string_view encoding) {
  if (absl::EqualsIgnoreCase(encoding, "hex")) {
    return xsel::query::BinaryEncoding::Hex;
  }
  if (absl::EqualsIgnoreCase(encoding, "base64")) {
    return xsel::query::BinaryEncoding::Base64;
  }
  if (absl::EqualsIgnoreCase(encoding, "raw")) {
    return xsel::query::BinaryEncoding::Raw;
  }
  std::cerr << "[xsel2] Warning: Unknown binary encoding '" << encoding
            << "', defaulting to 'hex'\n";
  return xsel::query::BinaryEncoding::Hex;
}

} // namespace

PYBIND11_MODULE(_scraperpy, m) {
  m.doc() = "High-performance HTML/XML scraping and JSON transformation engine";

#ifdef SCRAPERPY_VERSION_INFO
  m.attr("__version__") = SCRAPERPY_VERSION_INFO;
#else
  m.attr("__version__") = "0.5.0-dev";
#endif

  py::enum_<scraper::QueryAllStep::Type>(m, "Value")
      .value("Text", scraper::QueryAllStep::Type::Text)
      .value("OwnText", scraper::QueryAllStep::Type::OwnText)
      .value("Attribute", scraper::QueryAllStep::Type::Attribute)
      .value("ElementExists", scraper::QueryAllStep::Type::ElementExists)
      .value("AttributeExists", scraper::QueryAllStep::Type::AttributeExists)
      .value("Adjacent", scraper::QueryAllStep::Type::AdjacentText)
      .export_values();

  py::enum_<scraper::QueryStep::Number>(m, "Number")
      .value("All", scraper::QueryStep::Number::All)
      .value("Last", scraper::QueryStep::Number::Last)
      .value("Nth", scraper::QueryStep::Number::Nth)
      .export_values();

  py::enum_<scraper::InsertMode>(m, "InsertMode")
      .value("Default", scraper::InsertMode::Default)
      .value("Overwrite", scraper::InsertMode::Overwrite)
      .value("AppendToList", scraper::InsertMode::AppendToList)
      .value("UniqueSuffix", scraper::InsertMode::UniqueSuffix)
      .value("Merge", scraper::InsertMode::Merge)
      .export_values();

  py::class_<scraper::InsertOptions>(m, "InsertOptions")
      .def(py::init<>())
      .def_readwrite("mode", &scraper::InsertOptions::mode)
      .def_readwrite("allow_empty", &scraper::InsertOptions::allow_empty)
      .def_readwrite("nested_path", &scraper::InsertOptions::nested_path);

  py::class_<AhoCorasick>(m, "AhoCorasick")
      .def(py::init<>())
      .def("add_pattern", &AhoCorasick::AddPattern)
      .def("build", &AhoCorasick::Compile)
      .def("clear", &AhoCorasick::Clear)
      .def("pattern_count", &AhoCorasick::PatternCount)
      .def("contains_any", &AhoCorasick::HasAnyPattern)
      .def("has_all", &AhoCorasick::HasAllPatterns)
      .def("search", [](const AhoCorasick &ac, const std::string &text) {
        AhoCorasick::MatchList matches;
        ac.Find(text, matches);
        py::list out;
        for (const auto &match : matches) {
          out.append(py::make_tuple(match.start, match.start + match.length,
                                    text.substr(match.start, match.length),
                                    match.pattern_id));
        }
        return out;
      });

  py::class_<DocumentPy, std::shared_ptr<DocumentPy>>(m, "Document")
      .def_static("from_string", &DocumentPy::from_string, py::arg("content"))
      .def_static("from_file", &DocumentPy::from_file, py::arg("filename"));

  py::class_<ScraperPy>(m, "ScraperPy")
      .def(py::init<>())
      .def("from_string", &ScraperPy::from_string_py, py::arg("content"))
      .def("from_file", &ScraperPy::from_file_py, py::arg("filename"))
      .def("from_document", &ScraperPy::from_document_py, py::arg("document"))
      .def("select", &ScraperPy::Select, py::arg("selector"),
           py::arg("key") = "", py::return_value_policy::reference_internal)
      .def("text", &ScraperPy::Text, py::arg("own_text") = false,
           py::return_value_policy::reference_internal)
      .def(
          "attr",
          [](ScraperPy &self, const py::object &attr_name) -> ScraperPy & {
            if (py::isinstance<py::str>(attr_name)) {
              return self.Attr(attr_name.cast<std::string>());
            }
            return self.AttrFallback(py_str_or_list(attr_name));
          },
          py::arg("attr_name"), py::return_value_policy::reference_internal)
      .def("find_all", &ScraperPy::FindAll, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "", py::arg("optional") = false,
           py::return_value_policy::reference_internal)
      .def("find", &ScraperPy::Find, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "",
           py::arg("number") = scraper::QueryStep::Number::All,
           py::arg("nth_val") = 0, py::arg("optional") = false,
           py::return_value_policy::reference_internal)
      .def(
          "try_find",
          [](ScraperPy &self, const std::string &sel,
             scraper::QueryAllStep::Type type, const std::string &attr,
             scraper::QueryStep::Number number,
             size_t nth_val) -> ScraperPy & {
            return self.Find(sel, type, attr, number, nth_val, true);
          },
          py::arg("sel"), py::arg("type"), py::arg("attr") = "",
          py::arg("number") = scraper::QueryStep::Number::All,
          py::arg("nth_val") = 0, py::return_value_policy::reference_internal)
      .def(
          "find_first",
          [](ScraperPy &self, const std::string &sel,
             scraper::QueryAllStep::Type type, const py::object &attr,
             bool squish, bool required) -> ScraperPy & {
            return self.FindFirst(sel, type, py_str_or_list(attr), squish,
                                  required);
          },
          py::arg("sel"), py::arg("type") = scraper::QueryAllStep::Type::Text,
          py::arg("attr") = py::str(""), py::arg("squish") = true,
          py::arg("required") = false,
          py::return_value_policy::reference_internal)
      .def("extract_fields", &py_apply_extract_fields, py::arg("spec"),
           py::arg("allow_empty") = false,
           py::return_value_policy::reference_internal)
      .def("fields", &py_apply_extract_fields, py::arg("spec"),
           py::arg("allow_empty") = false,
           py::return_value_policy::reference_internal)
      .def("trim", &ScraperPy::Trim,
           py::return_value_policy::reference_internal)
      .def("squish", &ScraperPy::Squish,
           py::return_value_policy::reference_internal)
      .def("normalize_whitespace", &ScraperPy::Squish,
           py::return_value_policy::reference_internal)
      .def("lower", &ScraperPy::Lower,
           py::return_value_policy::reference_internal)
      .def("upper", &ScraperPy::Upper,
           py::return_value_policy::reference_internal)
      .def("unescape", &ScraperPy::Unescape, py::arg("url_decode") = true,
           py::return_value_policy::reference_internal)
      .def("replace", &ScraperPy::Replace, py::arg("from_str"),
           py::arg("to_str") = "", py::return_value_policy::reference_internal)
      .def("replace_exact", &ScraperPy::ReplaceExact, py::arg("from_str"),
           py::arg("to_str") = "", py::return_value_policy::reference_internal)
      .def("split", &ScraperPy::Split, py::arg("delimiter"),
           py::arg("keep_empty_parts") = false,
           py::return_value_policy::reference_internal)
      .def("join", &ScraperPy::Join, py::arg("delimiter"),
           py::return_value_policy::reference_internal)
      .def("prepend", &ScraperPy::Prepend, py::arg("prefix_str"),
           py::return_value_policy::reference_internal)
      .def("append", &ScraperPy::Append, py::arg("suffix_str"),
           py::return_value_policy::reference_internal)
      .def("drop_empty", &ScraperPy::DropEmpty, py::arg("also_trim") = true,
           py::return_value_policy::reference_internal)
      .def("substring", &ScraperPy::Substring, py::arg("start_pos"),
           py::arg("length") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("strip_comments", &ScraperPy::StripComments,
           py::arg("multiline") = true, py::arg("single_line") = true,
           py::return_value_policy::reference_internal)
      .def("regex_replace", &ScraperPy::RegexReplace, py::arg("pattern"),
           py::arg("replacement"), py::arg("flags") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("regex_filter", &ScraperPy::RegexFilter, py::arg("pattern"),
           py::arg("negate") = false, py::arg("flags") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("regex_extract", &ScraperPy::RegexExtract, py::arg("pattern"),
           py::arg("group_index") = 0, py::arg("flags") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("regex_find_all", &ScraperPy::RegexFindAll, py::arg("pattern"),
           py::arg("group_index") = 0, py::arg("flags") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("regex_groups", &ScraperPy::RegexGroups, py::arg("pattern"),
           py::arg("flags") = std::nullopt, py::arg("prefix") = "",
           py::arg("clear") = false, py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("filter", &ScraperPy::Filter, py::arg("patterns"),
           py::return_value_policy::reference_internal)
      .def(
          "set",
          [](ScraperPy &self, const std::string &name,
             scraper::InsertOptions opts,
             const std::string &type) -> ScraperPy & {
            return self.Set(name, opts, scraper::parse_value_type_name(type));
          },
          py::arg("name"), py::arg("opts") = scraper::InsertOptions{},
          py::arg("type") = "string",
          py::return_value_policy::reference_internal)
      .def("append_to", &ScraperPy::AppendTo, py::arg("key"),
           py::arg("allow_empty") = false,
           py::return_value_policy::reference_internal)
      .def("count", &ScraperPy::Count, py::arg("dict_key"),
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("first", &ScraperPy::First, py::arg("count") = 1,
           py::return_value_policy::reference_internal)
      .def("last", &ScraperPy::Last, py::arg("count") = 1,
           py::return_value_policy::reference_internal)
      .def("nth", &ScraperPy::Nth, py::arg("index"),
           py::return_value_policy::reference_internal)
      .def("slice", &ScraperPy::Slice, py::arg("start_index"),
           py::arg("end_index_inclusive"),
           py::return_value_policy::reference_internal)
      .def("unique", &ScraperPy::Unique, py::arg("preserve_order") = true,
           py::return_value_policy::reference_internal)
      .def("default", &ScraperPy::Default, py::arg("default_value"),
           py::return_value_policy::reference_internal)
      .def("js_block", &ScraperPy::JsBlock, py::arg("prefix") = "",
           py::return_value_policy::reference_internal)
      .def("extract_js_block", &ScraperPy::JsBlock, py::arg("prefix") = "",
           py::return_value_policy::reference_internal)
      .def("js_blocks", &py_apply_js_blocks, py::arg("prefixes"),
           py::arg("aliases") = py::none(),
           py::return_value_policy::reference_internal)
      .def("extract_js_blocks", &py_apply_js_blocks, py::arg("prefixes"),
           py::arg("aliases") = py::none(),
           py::return_value_policy::reference_internal)
      .def("parse_js", &ScraperPy::ParseJs, py::arg("objname"),
           py::arg("simplify") = true,
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("flatten", &ScraperPy::Flatten, py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("flatten_local", &ScraperPy::FlattenLocal,
           py::return_value_policy::reference_internal)
      .def("pick_keys", &ScraperPy::PickKeys, py::arg("keys_to_keep"),
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("omit_keys", &ScraperPy::OmitKeys, py::arg("keys_to_remove"),
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def(
          "debug",
          [](ScraperPy &self, const std::string &prefix, bool print_dict,
             bool print_node) -> ScraperPy & {
            return self.Debug(prefix, print_dict, print_node);
          },
          py::arg("prefix") = "DEBUG: ", py::arg("print_dict") = false,
          py::arg("print_node") = false,
          py::return_value_policy::reference_internal)
      .def("sort_keys", &ScraperPy::SortKeys, py::arg("ascending") = true,
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("deduplicate", &ScraperPy::Deduplicate,
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("jmespath", &ScraperPy::JmesPath, py::arg("expr"),
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("jsonpath", &ScraperPy::JsonPath, py::arg("expr"),
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def(
          "json_parse",
          [](ScraperPy &self, const std::string &objname, bool allow_comments,
             bool allow_trailing_comma, bool allow_inf_and_nan,
             bool lossless_number, int max_nesting_depth, bool strict,
             bool lenient) -> ScraperPy & {
            xsel::query::JsonParseOptions opts;
            opts.allow_comments = allow_comments;
            opts.allow_trailing_comma = allow_trailing_comma;
            opts.allow_inf_and_nan = allow_inf_and_nan;
            opts.lossless_number = lossless_number;
            opts.max_nesting_depth = max_nesting_depth;
            return self.JsonParse(objname, opts, strict,
                                  scraper::InsertOptions{}, lenient);
          },
          py::arg("objname") = "", py::arg("allow_comments") = false,
          py::arg("allow_trailing_comma") = false,
          py::arg("allow_inf_and_nan") = false,
          py::arg("lossless_number") = false,
          py::arg("max_nesting_depth") = 1024, py::arg("strict") = true,
          py::arg("lenient") = false,
          py::return_value_policy::reference_internal)
      .def("json_dump", &ScraperPy::JsonDump, py::arg("pretty") = false,
           py::arg("indent") = 2, py::arg("source_key") = "",
           py::return_value_policy::reference_internal)
      .def(
          "csv_parse",
          [](ScraperPy &self, const std::string &objname, bool assume_header,
             const std::string &delimiter, bool trim,
             bool strict) -> ScraperPy & {
            xsel::query::CsvOptions opts;
            opts.assume_header = assume_header;
            opts.delimiter = delimiter.empty() ? ',' : delimiter[0];
            opts.trim = trim;
            return self.CsvParse(objname, opts, strict);
          },
          py::arg("objname") = "", py::arg("assume_header") = true,
          py::arg("delimiter") = ",", py::arg("trim") = true,
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "csv_dump",
          [](ScraperPy &self, bool assume_header, const std::string &delimiter,
             bool trim, const std::string &source_key) -> ScraperPy & {
            xsel::query::CsvOptions opts;
            opts.assume_header = assume_header;
            opts.delimiter = delimiter.empty() ? ',' : delimiter[0];
            opts.trim = trim;
            return self.CsvDump(opts, source_key);
          },
          py::arg("assume_header") = true, py::arg("delimiter") = ",",
          py::arg("trim") = true, py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "jsonpath_replace",
          [](ScraperPy &self, const std::string &expr, py::object replacement,
             bool postponable) -> ScraperPy & {
            return self.JsonPathReplace(
                expr, xsel::python::python_to_json(replacement), postponable);
          },
          py::arg("expr"), py::arg("replacement"),
          py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def("json_flatten", &ScraperPy::JsonFlatten,
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("json_unflatten", &ScraperPy::JsonUnflatten,
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("jsonpointer_get", &ScraperPy::JsonPointerGet, py::arg("pointer"),
           py::arg("target_key") = "", py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def(
          "jsonpointer_set",
          [](ScraperPy &self, const std::string &pointer, py::object value,
             bool postponable) -> ScraperPy & {
            return self.JsonPointerSet(
                pointer, xsel::python::python_to_json(value), postponable);
          },
          py::arg("pointer"), py::arg("value"), py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def("jsonpointer_remove", &ScraperPy::JsonPointerRemove,
           py::arg("pointer"), py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def(
          "json_patch",
          [](ScraperPy &self, py::object patch,
             bool postponable) -> ScraperPy & {
            return self.JsonPatch(xsel::python::python_to_json(patch),
                                  postponable);
          },
          py::arg("patch"), py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def(
          "merge_patch",
          [](ScraperPy &self, py::object patch,
             bool postponable) -> ScraperPy & {
            return self.MergePatch(xsel::python::python_to_json(patch),
                                   postponable);
          },
          py::arg("patch"), py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def(
          "json_schema",
          [](ScraperPy &self, py::object schema, bool strict,
             const std::string &error_key, bool postponable) -> ScraperPy & {
            return self.JsonSchema(xsel::python::python_to_json(schema), strict,
                                   error_key, postponable);
          },
          py::arg("schema"), py::arg("strict") = true,
          py::arg("error_key") = "_schema_errors",
          py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def(
          "from_cbor",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            return self.FromCbor(objname, parse_py_binary_encoding(encoding),
                                 strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_cbor",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            return self.ToCbor(parse_py_binary_encoding(encoding), source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_msgpack",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            return self.FromMsgPack(objname, parse_py_binary_encoding(encoding),
                                    strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_msgpack",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            return self.ToMsgPack(parse_py_binary_encoding(encoding),
                                  source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_bson",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            return self.FromBson(objname, parse_py_binary_encoding(encoding),
                                 strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_bson",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            return self.ToBson(parse_py_binary_encoding(encoding), source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_ubjson",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            return self.FromUbjson(objname, parse_py_binary_encoding(encoding),
                                   strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_ubjson",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            return self.ToUbjson(parse_py_binary_encoding(encoding),
                                 source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def("parse_number", &ScraperPy::ParseNumber, py::arg("decimal") = ".",
           py::arg("thousands") = ",", py::arg("allow_float") = true,
           py::arg("key") = "", py::arg("opts") = scraper::InsertOptions{},
           py::arg("compact") = false,
           py::return_value_policy::reference_internal)
      .def("parse_duration", &ScraperPy::ParseDuration, py::arg("key") = "",
           py::arg("unit") = "seconds",
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def(
          "to_int",
          [](ScraperPy &self, const std::string &key,
             py::object default_val) -> ScraperPy & {
            std::optional<jsoncons::json> def;
            if (!default_val.is_none()) {
              def = xsel::python::python_to_json(default_val);
            }
            return self.ToInt(key, std::move(def));
          },
          py::arg("key") = "", py::arg("default_val") = py::none(),
          py::return_value_policy::reference_internal)
      .def(
          "to_float",
          [](ScraperPy &self, const std::string &key,
             py::object default_val) -> ScraperPy & {
            std::optional<jsoncons::json> def;
            if (!default_val.is_none()) {
              def = xsel::python::python_to_json(default_val);
            }
            return self.ToFloat(key, std::move(def));
          },
          py::arg("key") = "", py::arg("default_val") = py::none(),
          py::return_value_policy::reference_internal)
      .def(
          "to_bool",
          [](ScraperPy &self, const std::string &key,
             py::object default_val) -> ScraperPy & {
            std::optional<jsoncons::json> def;
            if (!default_val.is_none()) {
              def = xsel::python::python_to_json(default_val);
            }
            return self.ToBool(key, std::move(def));
          },
          py::arg("key") = "", py::arg("default_val") = py::none(),
          py::return_value_policy::reference_internal)
      .def(
          "cast_keys",
          [](ScraperPy &self,
             const std::unordered_map<std::string, std::string> &schema,
             bool postponable) -> ScraperPy & {
            absl::flat_hash_map<std::string, scraper::ValueType> typed;
            for (const auto &[k, v] : schema) {
              typed.emplace(k, scraper::parse_value_type_name(v));
            }
            return self.CastKeys(std::move(typed), postponable);
          },
          py::arg("schema"), py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def("resolve_url", &ScraperPy::ResolveUrl, py::arg("base_url"),
           py::return_value_policy::reference_internal)
      .def("url_part", &ScraperPy::UrlPart, py::arg("part"),
           py::return_value_policy::reference_internal)
      .def("query_param", &ScraperPy::QueryParam, py::arg("name"),
           py::arg("default_val") = "",
           py::return_value_policy::reference_internal)
      .def("strip_query", &ScraperPy::StripQuery,
           py::arg("keep_params") = std::vector<std::string>{},
           py::arg("drop_params") = std::vector<std::string>{},
           py::return_value_policy::reference_internal)
      .def("filter_urls", &ScraperPy::FilterUrls,
           py::arg("allowed_domains") = std::vector<std::string>{},
           py::arg("blocked_domains") = std::vector<std::string>{},
           py::arg("allowed_schemes") =
               std::vector<std::string>{"http", "https"},
           py::arg("same_origin_of") = "",
           py::return_value_policy::reference_internal)
      .def("slugify", &ScraperPy::Slugify, py::arg("separator") = "-",
           py::arg("max_len") = std::nullopt,
           py::return_value_policy::reference_internal)
      .def("html", &ScraperPy::Html, py::arg("outer") = false,
           py::return_value_policy::reference_internal)
      .def("tag_name", &ScraperPy::TagName,
           py::return_value_policy::reference_internal)
      .def("classes", &ScraperPy::Classes,
           py::return_value_policy::reference_internal)
      .def("attrs", &ScraperPy::Attrs, py::arg("data_only") = false,
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("strip_tags", &ScraperPy::StripTags,
           py::return_value_policy::reference_internal)
      .def("base64_encode", &ScraperPy::Base64Encode,
           py::arg("urlsafe") = false,
           py::return_value_policy::reference_internal)
      .def("base64_decode", &ScraperPy::Base64Decode,
           py::arg("urlsafe") = false,
           py::return_value_policy::reference_internal)
      .def("hash", &ScraperPy::Hash, py::arg("algo") = "fnv1a64",
           py::return_value_policy::reference_internal)
      .def(
          "replace_many",
          [](ScraperPy &self,
             const std::unordered_map<std::string, std::string> &mapping)
              -> ScraperPy & {
            absl::flat_hash_map<std::string, std::string> map(mapping.begin(),
                                                              mapping.end());
            return self.ReplaceMany(map);
          },
          py::arg("mapping"), py::return_value_policy::reference_internal)
      .def("extract_many", &ScraperPy::ExtractMany, py::arg("patterns"),
           py::arg("unique") = true,
           py::return_value_policy::reference_internal)
      .def("sort", &ScraperPy::Sort, py::arg("ascending") = true,
           py::arg("numeric") = false,
           py::return_value_policy::reference_internal)
      .def("reverse", &ScraperPy::Reverse,
           py::return_value_policy::reference_internal)
      .def("coalesce", &ScraperPy::Coalesce, py::arg("fallback") = "",
           py::return_value_policy::reference_internal)
      .def("zip_keys", &ScraperPy::ZipKeys, py::arg("keys_key"),
           py::arg("values_key"), py::arg("target_key") = "",
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("xpath", &ScraperPy::XPath, py::arg("expr"), py::arg("attr") = "",
           py::return_value_policy::reference_internal)
      .def("table", &ScraperPy::Table, py::arg("key") = "",
           py::arg("header") = true, py::arg("normalize_headers") = false,
           py::arg("trim_cells") = true,
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("extract_meta", &ScraperPy::ExtractMeta, py::arg("key") = "meta",
           py::arg("include_json_ld") = true,
           py::arg("opts") = scraper::InsertOptions{},
           py::arg("index_json_ld") = false,
           py::return_value_policy::reference_internal)
      .def("parse_date", &ScraperPy::ParseDate, py::arg("fmt") = "",
           py::arg("output") = "iso", py::arg("key") = "",
           py::arg("tz") = "UTC", py::arg("opts") = scraper::InsertOptions{},
           py::arg("now") = "", py::return_value_policy::reference_internal)
      .def("unicode_normalize", &ScraperPy::UnicodeNormalize,
           py::arg("form") = "NFC", py::return_value_policy::reference_internal)
      .def("strip_accents", &ScraperPy::StripAccents,
           py::return_value_policy::reference_internal)
      .def("fuzzy_filter", &ScraperPy::FuzzyFilter, py::arg("query"),
           py::arg("threshold") = 0.8, py::arg("algo") = "jaro_winkler",
           py::arg("case_sensitive") = false, py::arg("sort_by_score") = false,
           py::return_value_policy::reference_internal)
      .def("similarity", &ScraperPy::Similarity, py::arg("query"),
           py::arg("algo") = "jaro_winkler", py::arg("case_sensitive") = false,
           py::arg("key") = "", py::return_value_policy::reference_internal)
      .def("save", &ScraperPy::Save, py::arg("name"),
           py::return_value_policy::reference_internal)
      .def("restore", &ScraperPy::Restore, py::arg("name"),
           py::return_value_policy::reference_internal)
      .def(
          "coalesce_keys",
          [](ScraperPy &self, const py::dict &mapping, bool drop_sources,
             bool postponable) -> ScraperPy & {
            std::vector<scraper::CoalesceKeysStep::Rule> rules;
            rules.reserve(mapping.size());
            for (const auto &item : mapping) {
              rules.emplace_back(
                  item.first.cast<std::string>(),
                  py_str_or_list(
                      py::reinterpret_borrow<py::object>(item.second)));
            }
            return self.CoalesceKeys(std::move(rules), drop_sources,
                                     postponable);
          },
          py::arg("mapping"), py::arg("drop_sources") = true,
          py::arg("postponable") = true,
          py::return_value_policy::reference_internal)
      .def(
          "query_params",
          [](ScraperPy &self, const py::object &mapping,
             const std::string &default_val, bool clear) -> ScraperPy & {
            std::vector<scraper::QueryParamsStep::ParamMapping> pairs;
            if (py::isinstance<py::dict>(mapping)) {
              for (const auto &item : mapping.cast<py::dict>()) {
                pairs.emplace_back(item.first.cast<std::string>(),
                                   item.second.cast<std::string>());
              }
            } else {
              for (const auto &p : py_str_or_list(mapping)) {
                pairs.emplace_back(p, p);
              }
            }
            return self.QueryParams(std::move(pairs), default_val, clear);
          },
          py::arg("mapping"), py::arg("default_val") = "",
          py::arg("clear") = false, py::return_value_policy::reference_internal)
      .def(
          "map_values",
          [](ScraperPy &self,
             const std::unordered_map<std::string, std::string> &mapping,
             std::optional<std::string> default_val) -> ScraperPy & {
            absl::flat_hash_map<std::string, std::string> map(mapping.begin(),
                                                              mapping.end());
            return self.MapValues(std::move(map), std::move(default_val));
          },
          py::arg("mapping"), py::arg("default_val") = std::nullopt,
          py::return_value_policy::reference_internal)
      .def("skip_if_exists", &ScraperPy::SkipIfExists, py::arg("selector"),
           py::return_value_policy::reference_internal)
      .def("skip_if_empty", &ScraperPy::SkipIfEmpty,
           py::return_value_policy::reference_internal)
      .def("abort_if_exists", &ScraperPy::AbortIfExists, py::arg("selector"),
           py::arg("key") = "", py::arg("reason") = "",
           py::return_value_policy::reference_internal)
      .def(
          "include",
          [](ScraperPy &self, const ScraperPy &other) -> ScraperPy & {
            return self.Include(other);
          },
          py::arg("other"), py::return_value_policy::reference_internal)
      .def("unescape_html", &ScraperPy::UnescapeHtml,
           py::return_value_policy::reference_internal)
      .def("parse_srcset", &ScraperPy::ParseSrcset,
           py::arg("mode") = "largest",
           py::return_value_policy::reference_internal)
      .def("css_url", &ScraperPy::CssUrl,
           py::return_value_policy::reference_internal);

  m.def(
      "parse_js",
      [](const std::string &source, bool simplify) {
        return xsel::python::json_to_python(
            digger::parse_object(source, simplify));
      },
      py::arg("source"), py::arg("simplify") = true);

  m.def(
      "json_parse",
      [](const std::string &text, bool allow_comments,
         bool allow_trailing_comma, bool allow_inf_and_nan,
         bool lossless_number, int max_nesting_depth, bool lenient,
         bool simplify) {
        if (lenient) {
          return xsel::python::json_to_python(
              digger::parse_object(text, simplify));
        }
        xsel::query::JsonParseOptions opts;
        opts.allow_comments = allow_comments;
        opts.allow_trailing_comma = allow_trailing_comma;
        opts.allow_inf_and_nan = allow_inf_and_nan;
        opts.lossless_number = lossless_number;
        opts.max_nesting_depth = max_nesting_depth;
        return xsel::python::json_to_python(
            xsel::query::parse_json(text, opts));
      },
      py::arg("text"), py::arg("allow_comments") = false,
      py::arg("allow_trailing_comma") = false,
      py::arg("allow_inf_and_nan") = false, py::arg("lossless_number") = false,
      py::arg("max_nesting_depth") = 1024, py::arg("lenient") = false,
      py::arg("simplify") = true);

  m.def("jmespath_eval", [](py::object data, const std::string &expr) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    return xsel::python::json_to_python(
        xsel::query::evaluate_jmespath(doc, expr));
  });

  m.def("jsonpath_query", [](py::object data, const std::string &expr) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    return xsel::python::json_to_python(
        xsel::query::evaluate_jsonpath(doc, expr));
  });

  m.def("json_patch_apply", [](py::object data, py::object patch) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    const jsoncons::json p = xsel::python::python_to_json(patch);
    return xsel::python::json_to_python(xsel::query::apply_json_patch(doc, p));
  });

  m.def("merge_patch_apply", [](py::object data, py::object patch) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    const jsoncons::json p = xsel::python::python_to_json(patch);
    return xsel::python::json_to_python(xsel::query::apply_merge_patch(doc, p));
  });

  m.def("json_schema_validate", [](py::object data, py::object schema) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    const jsoncons::json s = xsel::python::python_to_json(schema);
    std::vector<std::string> errors;
    const bool ok = xsel::query::validate_json_schema(doc, s, &errors);
    return py::make_tuple(ok, errors);
  });

  m.def("jsonpointer_get", [](py::object data, const std::string &pointer) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    return xsel::python::json_to_python(
        xsel::query::jsonpointer_get(doc, pointer));
  });

  m.def("json_flatten", [](py::object data) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    return xsel::python::json_to_python(xsel::query::json_flatten(doc));
  });

  m.def("json_unflatten", [](py::object data) {
    const jsoncons::json doc = xsel::python::python_to_json(data);
    return xsel::python::json_to_python(xsel::query::json_unflatten(doc));
  });

  m.def("parse_dsl_string", [](const std::string &dsl_content) {
    scraper::dsl::DslParser parser;
    auto parsed = parser.parse(dsl_content);
    return scrapers_to_py_dict(parsed);
  });

  m.def("load_scrapers", [](const std::string &filepath) {
    scraper::dsl::DslParser parser;
    auto parsed = parser.parse_file(filepath);
    return scrapers_to_py_dict(parsed);
  });

  m.def(
      "scrape_many",
      [](const py::dict &scrapers, const std::string &html) {
        Document doc;
        doc.parseMemory(html);
        py::dict out;
        for (const auto &item : scrapers) {
          ScraperPy &s = item.second.cast<ScraperPy &>();
          jsoncons::json res = jsoncons::json::object();
          if (doc.isValid()) {
            s.scrape_document(doc, res, item.first.cast<std::string>());
          }
          out[item.first] = xsel::python::json_to_python(res);
        }
        return out;
      },
      py::arg("scrapers"), py::arg("html"));
}
