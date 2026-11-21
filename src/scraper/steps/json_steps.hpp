#pragma once

#include "query/json_query.hpp"
#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <string>
#include <string_view>

namespace scraper {

class JsonParseStep final : public Step {
public:
  JsonParseStep(std::string_view objname = "",
                xsel::query::JsonParseOptions opts = {}, bool strict = true,
                InsertOptions insert_opts = {}, bool lenient = false)
      : objname_(objname), opts_(opts), strict_(strict),
        insert_opts_(insert_opts), lenient_(lenient) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string objname_;
  xsel::query::JsonParseOptions opts_;
  bool strict_;
  InsertOptions insert_opts_;
  bool lenient_;
};

class JsonDumpStep final : public Step {
public:
  JsonDumpStep(bool pretty = false, int indent = 2,
               std::string_view source_key = "")
      : pretty_(pretty), indent_(indent), source_key_(source_key) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool pretty_;
  int indent_;
  std::string source_key_;
};

class CsvParseStep final : public Step {
public:
  CsvParseStep(std::string_view objname = "",
               xsel::query::CsvOptions opts = {}, bool strict = true,
               InsertOptions insert_opts = {})
      : objname_(objname), opts_(opts), strict_(strict),
        insert_opts_(insert_opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string objname_;
  xsel::query::CsvOptions opts_;
  bool strict_;
  InsertOptions insert_opts_;
};

class CsvDumpStep final : public Step {
public:
  CsvDumpStep(xsel::query::CsvOptions opts = {},
              std::string_view source_key = "")
      : opts_(opts), source_key_(source_key) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  xsel::query::CsvOptions opts_;
  std::string source_key_;
};

class JsonSchemaStep final : public JsonTransformStep {
public:
  explicit JsonSchemaStep(jsoncons::json schema, bool strict = true,
                          std::string_view error_key = "_schema_errors")
      : schema_(std::move(schema)),
        strict_(strict),
        error_key_(error_key) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  jsoncons::json schema_;
  bool strict_;
  std::string error_key_;
};

} // namespace scraper
