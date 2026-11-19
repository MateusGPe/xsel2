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
      .def("push", &ScraperPy::Group, py::arg("selector"), py::arg("key") = "",
           py::return_value_policy::reference_internal)
      .def("text", &ScraperPy::Text, py::arg("own_text") = false,
           py::return_value_policy::reference_internal)
      .def("attr", &ScraperPy::Attr, py::arg("attr_name"),
           py::return_value_policy::reference_internal)
      .def("query_all", &ScraperPy::QueryAll, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "", py::return_value_policy::reference_internal)
      .def("query", &ScraperPy::Query, py::arg("sel"), py::arg("type"),
           py::arg("attr") = "",
           py::arg("number") = scraper::QueryStep::Number::All,
           py::arg("nth_val") = 0, py::return_value_policy::reference_internal)
      .def("trim", &ScraperPy::Trim, py::return_value_policy::reference_internal)
      .def("strip", &ScraperPy::Normspace,
           py::return_value_policy::reference_internal)
      .def("normspace", &ScraperPy::Normspace,
           py::return_value_policy::reference_internal)
      .def("lower", &ScraperPy::Lower,
           py::return_value_policy::reference_internal)
      .def("upper", &ScraperPy::Upper,
           py::return_value_policy::reference_internal)
      .def("unescape", &ScraperPy::Unescape, py::arg("url_decode") = true,
           py::return_value_policy::reference_internal)
      .def("replace", &ScraperPy::Replace, py::arg("from_str"),
           py::arg("to_str") = "", py::return_value_policy::reference_internal)
      .def("substitute", &ScraperPy::Substitute, py::arg("from_str"),
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
      .def("remove_empty", &ScraperPy::RemoveEmpty, py::arg("also_trim") = true,
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
      .def("count_values", &ScraperPy::CountValues, py::arg("dict_key"),
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("first", &ScraperPy::First, py::arg("count") = 1,
           py::return_value_policy::reference_internal)
      .def("last", &ScraperPy::Last, py::arg("count") = 1,
           py::return_value_policy::reference_internal)
      .def("nth", &ScraperPy::Nth, py::arg("index"),
           py::return_value_policy::reference_internal)
      .def("slice_values", &ScraperPy::SliceValues, py::arg("start_index"),
           py::arg("end_index_inclusive"),
           py::return_value_policy::reference_internal)
      .def("unique", &ScraperPy::Unique, py::arg("preserve_order") = true,
           py::return_value_policy::reference_internal)
      .def("default_if_empty", &ScraperPy::DefaultIfEmpty,
           py::arg("default_value"), py::return_value_policy::reference_internal)
      .def("block_seeker", &ScraperPy::BlockSeeker, py::arg("prefix") = "",
           py::return_value_policy::reference_internal)
      .def("block_parser", &ScraperPy::BlockParser, py::arg("prefixes"),
           py::return_value_policy::reference_internal)
      .def("parse", &ScraperPy::Parse, py::arg("objname"),
           py::arg("simplify") = true,
           py::arg("opts") = scraper::InsertOptions{},
           py::return_value_policy::reference_internal)
      .def("flatten_dict", &ScraperPy::FlattenDict,
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("flatten_local", &ScraperPy::FlattenLocal,
           py::return_value_policy::reference_internal)
      .def("filter_dict", &ScraperPy::FilterDict, py::arg("keys_to_keep"),
           py::arg("postponable") = true,
           py::return_value_policy::reference_internal)
      .def("remove_keys", &ScraperPy::RemoveKeys, py::arg("keys_to_remove"),
           py::arg("postponable") = true,
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
