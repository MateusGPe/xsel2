#include "scraper/steps/json_steps.hpp"
#include "common/json_utils.hpp"
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

void store_parsed_json(jsoncons::json &dict, const std::string &objname,
                       jsoncons::json &&parsed,
                       const InsertOptions &insert_opts) {
  if (xsel::common::is_empty_json(parsed) && !insert_opts.allow_empty) {
    return;
  }
  if (objname.empty()) {
    merge_into(dict, std::move(parsed), insert_opts.mode);
  } else {
    insert_into_json(dict, objname, std::move(parsed), insert_opts);
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

} // namespace

void JsonParseStep::apply(Node, std::vector<std::string> &values,
                          jsoncons::json &dict) {
  if (values.empty()) {
    return;
  }
  const std::string combined = join_values(values);
  values.clear();
  try {
    jsoncons::json parsed = xsel::query::parse_json(combined, opts_);
    store_parsed_json(dict, objname_, std::move(parsed), insert_opts_);
  } catch (const std::exception &) {
    if (strict_) {
      throw;
    }
  }
}

void JsonDumpStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &dict) {
  const auto &src = select_source_json(dict, source_key_);
  values.clear();
  values.push_back(xsel::query::dump_json(src, pretty_, indent_));
}

void CsvParseStep::apply(Node, std::vector<std::string> &values,
                         jsoncons::json &dict) {
  if (values.empty()) {
    return;
  }
  const std::string combined = join_values(values);
  values.clear();
  try {
    jsoncons::json parsed = xsel::query::decode_csv(combined, opts_);
    store_parsed_json(dict, objname_, std::move(parsed), insert_opts_);
  } catch (const std::exception &) {
    if (strict_) {
      throw;
    }
  }
}

void CsvDumpStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &dict) {
  const auto &src = select_source_json(dict, source_key_);
  values.clear();
  values.push_back(xsel::query::encode_csv(src, opts_));
}

void BinaryDecodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &dict) {
  if (values.empty()) {
    return;
  }
  const std::string combined = join_values(values);
  values.clear();
  try {
    jsoncons::json parsed = xsel::query::decode_binary(fmt_, combined, enc_);
    store_parsed_json(dict, objname_, std::move(parsed), insert_opts_);
  } catch (const std::exception &) {
    if (strict_) {
      throw;
    }
  }
}

void BinaryEncodeStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &dict) {
  const auto &src = select_source_json(dict, source_key_);
  values.clear();
  values.push_back(xsel::query::encode_binary(fmt_, src, enc_));
}

void JsonPathReplaceStep::apply(Node, std::vector<std::string> &,
                                jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonPathReplaceStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::jsonpath_replace(dest, expr_, replacement_);
}

void JsonFlattenStep::apply(Node, std::vector<std::string> &,
                            jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonFlattenStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::json_flatten(dest);
}

void JsonUnflattenStep::apply(Node, std::vector<std::string> &,
                              jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonUnflattenStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::json_unflatten(dest);
}

void JsonPointerGetStep::apply(Node, std::vector<std::string> &,
                               jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonPointerGetStep::execute_postponed(const jsoncons::json &dest) const {
  jsoncons::json val = xsel::query::jsonpointer_get(dest, pointer_);
  if (target_key_.empty()) {
    return val;
  }
  jsoncons::json out = dest.is_object() ? dest : jsoncons::json::object();
  insert_into_json(out, target_key_, std::move(val),
                   InsertOptions{InsertMode::Overwrite, true, true});
  return out;
}

void JsonPointerSetStep::apply(Node, std::vector<std::string> &,
                               jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonPointerSetStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::jsonpointer_set(dest, pointer_, value_);
}

void JsonPointerRemoveStep::apply(Node, std::vector<std::string> &,
                                  jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonPointerRemoveStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::jsonpointer_remove(dest, pointer_);
}

void JsonPatchStep::apply(Node, std::vector<std::string> &,
                          jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonPatchStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::apply_json_patch(dest, patch_);
}

void MergePatchStep::apply(Node, std::vector<std::string> &,
                           jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
MergePatchStep::execute_postponed(const jsoncons::json &dest) const {
  return xsel::query::apply_merge_patch(dest, patch_);
}

void JsonSchemaStep::apply(Node, std::vector<std::string> &,
                           jsoncons::json &dict) {
  if (!postponable_) {
    dict = execute_postponed(dict);
  }
}

jsoncons::json
JsonSchemaStep::execute_postponed(const jsoncons::json &dest) const {
  std::vector<std::string> errors;
  if (xsel::query::validate_json_schema(dest, schema_, &errors)) {
    return dest;
  }
  if (strict_) {
    std::string msg = "JSON Schema validation failed:";
    for (const auto &err : errors) {
      msg += "\n  - " + err;
    }
    throw std::runtime_error(msg);
  }
  jsoncons::json out = dest;
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
