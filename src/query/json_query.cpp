#include "query/json_query.hpp"
#include "common/string_utils.hpp"
#include <cstdint>
#include <jsoncons_ext/csv/csv.hpp>
#include <jsoncons_ext/jmespath/jmespath.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace xsel::query {

namespace {

jsoncons::csv::csv_options make_csv_options(const CsvOptions &opts) {
  jsoncons::csv::csv_options copts;
  copts.assume_header(opts.assume_header)
      .field_delimiter(opts.delimiter)
      .trim(opts.trim);
  return copts;
}

} // namespace

jsoncons::json evaluate_jmespath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jmespath::search(doc, expr);
  } catch (const jsoncons::jmespath::jmespath_error &e) {
    throw std::runtime_error(std::string("JMESPath error: ") + e.what());
  }
}

jsoncons::json evaluate_jsonpath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jsonpath::json_query(doc, expr);
  } catch (const jsoncons::jsonpath::jsonpath_error &e) {
    throw std::runtime_error(std::string("JSONPath error: ") + e.what());
  }
}

jsoncons::json evaluate(QueryDialect dialect, const jsoncons::json &doc,
                        std::string_view expr) {
  switch (dialect) {
  case QueryDialect::JmesPath:
    return evaluate_jmespath(doc, expr);
  case QueryDialect::JsonPath:
    return evaluate_jsonpath(doc, expr);
  }
  throw std::runtime_error("Unsupported QueryDialect");
}

jsoncons::json parse_json(std::string_view input,
                          const JsonParseOptions &opts) {
  try {
    jsoncons::json_options jopts;
    jopts.max_nesting_depth(opts.max_nesting_depth);
    jopts.lossless_number(opts.lossless_number);
    if (opts.allow_trailing_comma) {
      jopts.err_handler(jsoncons::allow_trailing_commas());
    }
    if (opts.allow_inf_and_nan) {
      jopts.nan_to_num("NaN").inf_to_num("Inf").neginf_to_num("-Inf");
    }
    if (opts.allow_comments) {
      const std::string stripped =
          xsel::common::strip_c_comments(input, true, true, true);
      return jsoncons::json::parse(stripped, jopts);
    }
    return jsoncons::json::parse(input, jopts);
  } catch (const jsoncons::ser_error &e) {
    throw std::runtime_error(std::string("JSON parse error: ") + e.what());
  }
}

std::string dump_json(const jsoncons::json &doc, bool pretty, int indent) {
  std::string out;
  if (pretty) {
    jsoncons::json_options opts;
    opts.indent_size(static_cast<uint8_t>(indent < 0 ? 0 : (indent > 16 ? 16 : indent)));
    jsoncons::encode_json_pretty(doc, out, opts);
  } else {
    jsoncons::encode_json(doc, out);
  }
  return out;
}

bool validate_json_schema(const jsoncons::json &doc,
                          const jsoncons::json &schema,
                          std::vector<std::string> *errors_out) {
  try {
    auto compiled = jsoncons::jsonschema::make_json_schema(schema);
    bool valid = true;
    auto reporter = [&](const jsoncons::jsonschema::validation_message &msg) {
      valid = false;
      if (errors_out) {
        errors_out->push_back(msg.instance_location().string() + ": " +
                              msg.message());
      }
      return jsoncons::jsonschema::walk_result::advance;
    };
    compiled.validate(doc, reporter);
    return valid;
  } catch (const jsoncons::jsonschema::schema_error &e) {
    throw std::runtime_error(std::string("JSON Schema error: ") + e.what());
  }
}

jsoncons::json decode_csv(std::string_view input, const CsvOptions &opts) {
  try {
    return jsoncons::csv::decode_csv<jsoncons::json>(input,
                                                     make_csv_options(opts));
  } catch (const jsoncons::ser_error &e) {
    throw std::runtime_error(std::string("CSV decode error: ") + e.what());
  }
}

std::string encode_csv(const jsoncons::json &doc, const CsvOptions &opts) {
  try {
    std::string out;
    jsoncons::csv::encode_csv(doc, out, make_csv_options(opts));
    return out;
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("CSV encode error: ") + e.what());
  }
}

} // namespace xsel::query
