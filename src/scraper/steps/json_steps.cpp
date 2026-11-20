#include "scraper/steps/json_steps.hpp"
#include "common/json_utils.hpp"
#include "digger/digger.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace scraper {

namespace {

std::string join_values(const std::vector<std::string> &values) {
  std::string combined;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      combined.push_back('\n');
    }
    combined += values[i];
  }
  return combined;
}

template <typename ParseFn>
void parse_and_store(std::vector<std::string> &values, jsoncons::json &dict,
                     const std::string &objname, bool strict,
                     const InsertOptions &insert_opts, ParseFn &&parse_fn) {
  if (values.empty()) {
    return;
  }
  const std::string combined = join_values(values);
  values.clear();
  try {
    jsoncons::json parsed = parse_fn(combined);
    if (xsel::common::is_empty_json(parsed) && !insert_opts.allow_empty) {
      return;
    }
    if (objname.empty()) {
      merge_into(dict, std::move(parsed), insert_opts.mode);
    } else {
      insert_into_json(dict, objname, std::move(parsed), insert_opts);
    }
  } catch (const std::exception &) {
    if (strict) {
      throw;
    }
  }
}

const jsoncons::json &select_source_json(const jsoncons::json &dict,
                                         const std::string &source_key) {
  if (source_key.empty()) {
    return dict;
  }
  if (!dict.is_object() || !dict.contains(source_key)) {
    static const jsoncons::json kNull = jsoncons::json::null();
    return kNull;
  }
  return dict[source_key];
}

template <typename DumpFn>
void dump_from_dict(std::vector<std::string> &values, const jsoncons::json &dict,
                    const std::string &source_key, DumpFn &&dump_fn) {
  const auto &src = select_source_json(dict, source_key);
  values.clear();
  values.push_back(dump_fn(src));
}

} // namespace

void JsonParseStep::apply(Node, std::vector<std::string> &values,
                          jsoncons::json &dict) {
  parse_and_store(values, dict, objname_, strict_, insert_opts_,
                  [this](std::string_view input) {
                    if (lenient_) {
                      return digger::parse_object(input, /*simplify=*/true);
                    }
                    return xsel::query::parse_json(input, opts_);
                  });
}

void JsonDumpStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &dict) {
  dump_from_dict(values, dict, source_key_, [this](const jsoncons::json &src) {
    return xsel::query::dump_json(src, pretty_, indent_);
  });
}

void CsvParseStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &dict) {
  parse_and_store(values, dict, objname_, strict_, insert_opts_,
                  [this](std::string_view input) {
                    return xsel::query::decode_csv(input, opts_);
                  });
}

void CsvDumpStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &dict) {
  dump_from_dict(values, dict, source_key_, [this](const jsoncons::json &src) {
    return xsel::query::encode_csv(src, opts_);
  });
}

void BinaryDecodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &dict) {
  parse_and_store(values, dict, objname_, strict_, insert_opts_,
                  [this](std::string_view input) {
                    return xsel::query::decode_binary(fmt_, input, enc_);
                  });
}

void BinaryEncodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &dict) {
  dump_from_dict(values, dict, source_key_, [this](const jsoncons::json &src) {
    return xsel::query::encode_binary(fmt_, src, enc_);
  });
}

jsoncons::json
JsonPathReplaceStep::transform(const jsoncons::json &input) const {
  return xsel::query::jsonpath_replace(input, expr_, replacement_);
}

jsoncons::json
JsonFlattenStep::transform(const jsoncons::json &input) const {
  return xsel::query::json_flatten(input);
}

jsoncons::json
JsonUnflattenStep::transform(const jsoncons::json &input) const {
  return xsel::query::json_unflatten(input);
}

jsoncons::json
JsonPointerGetStep::transform(const jsoncons::json &input) const {
  jsoncons::json val = xsel::query::jsonpointer_get(input, pointer_);
  if (target_key_.empty()) {
    return val;
  }
  jsoncons::json out = input.is_object() ? input : jsoncons::json::object();
  insert_into_json(out, target_key_, std::move(val),
                   InsertOptions{InsertMode::Overwrite, true, true});
  return out;
}

jsoncons::json
JsonPointerSetStep::transform(const jsoncons::json &input) const {
  return xsel::query::jsonpointer_set(input, pointer_, value_);
}

jsoncons::json
JsonPointerRemoveStep::transform(const jsoncons::json &input) const {
  return xsel::query::jsonpointer_remove(input, pointer_);
}

jsoncons::json JsonPatchStep::transform(const jsoncons::json &input) const {
  return xsel::query::apply_json_patch(input, patch_);
}

jsoncons::json MergePatchStep::transform(const jsoncons::json &input) const {
  return xsel::query::apply_merge_patch(input, patch_);
}

jsoncons::json JsonSchemaStep::transform(const jsoncons::json &input) const {
  std::vector<std::string> errors;
  if (xsel::query::validate_json_schema(input, schema_, &errors)) {
    return input;
  }
  if (strict_) {
    std::string msg = "JSON Schema validation failed:";
    for (const auto &err : errors) {
      msg += "\n  - " + err;
    }
    throw std::runtime_error(msg);
  }
  jsoncons::json out = input;
  if (out.is_object() && !error_key_.empty()) {
    jsoncons::json err_arr = jsoncons::json::array();
    for (const auto &err : errors) {
      err_arr.push_back(err);
    }
    out.insert_or_assign(error_key_, std::move(err_arr));
  }
  return out;
}

} // namespace scraper
