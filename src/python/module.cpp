#include "common/ahocorasick.hpp"
#include "python/json_py_bridge.hpp"
#include "query/json_query.hpp"
#include "scraper/dsl_parser.hpp"
#include "scraper/scraper.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

class ScraperPy final : public scraper::ScraperBuilderBase<ScraperPy> {
public:
  using scraper::ScraperBuilderBase<ScraperPy>::ScraperBuilderBase;

  py::object from_string_py(const std::string &content) {
    return xsel::python::json_to_python(scrape_from_string(content));
  }

  py::object from_file_py(const std::string &filename) {
    return xsel::python::json_to_python(scrape_from_file(filename));
  }
};

} // namespace

PYBIND11_MODULE(_scraperpy, m) {
  m.doc() = "High-performance HTML/XML scraping and JSON transformation engine";

#ifdef SCRAPERPY_VERSION_INFO
  m.attr("__version__") = SCRAPERPY_VERSION_INFO;
#else
  m.attr("__version__") = "0.4.0-dev";
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
      .def("contains_any", &AhoCorasick::HasAnyPattern)
      .def("search", [](const AhoCorasick &ac, const std::string &text) {
        AhoCorasick::MatchList matches;
        ac.Find(text, matches);
        py::list out;
        for (const auto &match : matches) {
          out.append(py::make_tuple(
              match.start, match.start + match.length,
              text.substr(match.start, match.length), match.pattern_id));
        }
        return out;
      });

  py::class_<ScraperPy>(m, "ScraperPy")
      .def(py::init<>())
      .def("from_string", &ScraperPy::from_string_py, py::arg("content"))
      .def("from_file", &ScraperPy::from_file_py, py::arg("filename"))
      .def("select", &ScraperPy::Select, py::arg("selector"), py::arg("key") = "",
           py::return_value_policy::reference_internal)
      .def("text", &ScraperPy::Text, py::arg("own_text") = false,
           py::return_value_policy::reference_internal)
      .def("attr", &ScraperPy::Attr, py::arg("attr_name"),
           py::return_value_policy::reference_internal)
      .def("find_all", &ScraperPy::FindAll, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "", py::return_value_policy::reference_internal)
      .def("find", &ScraperPy::Find, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "",
           py::arg("number") = scraper::QueryStep::Number::All,
           py::arg("nth_val") = 0, py::return_value_policy::reference_internal)
      .def("trim", &ScraperPy::Trim, py::return_value_policy::reference_internal)
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
      .def("filter", &ScraperPy::Filter, py::arg("patterns"),
           py::return_value_policy::reference_internal)
      .def("set", &ScraperPy::Set, py::arg("name"),
           py::arg("opts") = scraper::InsertOptions{},
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
      .def("default", &ScraperPy::Default,
           py::arg("default_value"), py::return_value_policy::reference_internal)
      .def("js_block", &ScraperPy::JsBlock, py::arg("prefix") = "",
           py::return_value_policy::reference_internal)
      .def("extract_js_block", &ScraperPy::JsBlock, py::arg("prefix") = "",
           py::return_value_policy::reference_internal)
      .def("js_blocks", &ScraperPy::JsBlocks, py::arg("prefixes"),
           py::return_value_policy::reference_internal)
      .def("extract_js_blocks", &ScraperPy::JsBlocks, py::arg("prefixes"),
           py::return_value_policy::reference_internal)
      .def("parse_js", &ScraperPy::ParseJs, py::arg("objname"),
           py::arg("simplify") = true,
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("flatten", &ScraperPy::Flatten,
           py::arg("postponable") = true,
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
             bool lossless_number, int max_nesting_depth,
             bool strict) -> ScraperPy & {
            xsel::query::JsonParseOptions opts;
            opts.allow_comments = allow_comments;
            opts.allow_trailing_comma = allow_trailing_comma;
            opts.allow_inf_and_nan = allow_inf_and_nan;
            opts.lossless_number = lossless_number;
            opts.max_nesting_depth = max_nesting_depth;
            return self.JsonParse(objname, opts, strict);
          },
          py::arg("objname") = "", py::arg("allow_comments") = false,
          py::arg("allow_trailing_comma") = false,
          py::arg("allow_inf_and_nan") = false,
          py::arg("lossless_number") = false,
          py::arg("max_nesting_depth") = 1024, py::arg("strict") = true,
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
          py::arg("strict") = true,
          py::return_value_policy::reference_internal)
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
          py::arg("expr"), py::arg("replacement"), py::arg("postponable") = true,
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
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.FromCbor(objname, enc, strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_cbor",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.ToCbor(enc, source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_msgpack",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.FromMsgPack(objname, enc, strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_msgpack",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.ToMsgPack(enc, source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_bson",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.FromBson(objname, enc, strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_bson",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.ToBson(enc, source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal)
      .def(
          "from_ubjson",
          [](ScraperPy &self, const std::string &objname,
             const std::string &encoding, bool strict) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.FromUbjson(objname, enc, strict);
          },
          py::arg("objname") = "", py::arg("encoding") = "hex",
          py::arg("strict") = true, py::return_value_policy::reference_internal)
      .def(
          "to_ubjson",
          [](ScraperPy &self, const std::string &encoding,
             const std::string &source_key) -> ScraperPy & {
            auto enc = encoding == "base64" ? xsel::query::BinaryEncoding::Base64
                       : encoding == "raw"  ? xsel::query::BinaryEncoding::Raw
                                            : xsel::query::BinaryEncoding::Hex;
            return self.ToUbjson(enc, source_key);
          },
          py::arg("encoding") = "hex", py::arg("source_key") = "",
          py::return_value_policy::reference_internal);

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
    py::dict out;
    for (auto &[name, builder] : parsed) {
      ScraperPy py_scraper;
      py_scraper.set_extractors(builder.get_extractors());
      out[py::str(name)] = py::cast(std::move(py_scraper));
    }
    return out;
  });

  m.def("load_scrapers", [](const std::string &filepath) {
    scraper::dsl::DslParser parser;
    auto parsed = parser.parse_file(filepath);
    py::dict out;
    for (auto &[name, builder] : parsed) {
      ScraperPy py_scraper;
      py_scraper.set_extractors(builder.get_extractors());
      out[py::str(name)] = py::cast(std::move(py_scraper));
    }
    return out;
  });
}
