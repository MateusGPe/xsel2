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
                InsertOptions insert_opts = {})
      : objname_(objname), opts_(opts), strict_(strict),
        insert_opts_(insert_opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string objname_;
  xsel::query::JsonParseOptions opts_;
  bool strict_;
  InsertOptions insert_opts_;
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

class BinaryDecodeStep final : public Step {
public:
  BinaryDecodeStep(
      xsel::query::BinaryFormat fmt, std::string_view objname = "",
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      bool strict = true, InsertOptions insert_opts = {})
      : fmt_(fmt), objname_(objname), enc_(enc), strict_(strict),
        insert_opts_(insert_opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  xsel::query::BinaryFormat fmt_;
  std::string objname_;
  xsel::query::BinaryEncoding enc_;
  bool strict_;
  InsertOptions insert_opts_;
};

class BinaryEncodeStep final : public Step {
public:
  BinaryEncodeStep(
      xsel::query::BinaryFormat fmt,
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      std::string_view source_key = "")
      : fmt_(fmt), enc_(enc), source_key_(source_key) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  xsel::query::BinaryFormat fmt_;
  xsel::query::BinaryEncoding enc_;
  std::string source_key_;
};

class JsonPathReplaceStep final : public PostponableJsonStep {
public:
  JsonPathReplaceStep(std::string_view expr, jsoncons::json replacement,
                      bool postponable = true)
      : PostponableJsonStep(postponable),
        expr_(expr),
        replacement_(std::move(replacement)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string expr_;
  jsoncons::json replacement_;
};

class JsonFlattenStep final : public PostponableJsonStep {
public:
  explicit JsonFlattenStep(bool postponable = true) noexcept
      : PostponableJsonStep(postponable) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;
};

class JsonUnflattenStep final : public PostponableJsonStep {
public:
  explicit JsonUnflattenStep(bool postponable = true) noexcept
      : PostponableJsonStep(postponable) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;
};

class JsonPointerGetStep final : public PostponableJsonStep {
public:
  JsonPointerGetStep(std::string_view pointer, std::string_view target_key = "",
                     bool postponable = true)
      : PostponableJsonStep(postponable),
        pointer_(pointer),
        target_key_(target_key) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string pointer_;
  std::string target_key_;
};

class JsonPointerSetStep final : public PostponableJsonStep {
public:
  JsonPointerSetStep(std::string_view pointer, jsoncons::json value,
                     bool postponable = true)
      : PostponableJsonStep(postponable),
        pointer_(pointer),
        value_(std::move(value)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string pointer_;
  jsoncons::json value_;
};

class JsonPointerRemoveStep final : public PostponableJsonStep {
public:
  explicit JsonPointerRemoveStep(std::string_view pointer,
                                 bool postponable = true)
      : PostponableJsonStep(postponable), pointer_(pointer) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string pointer_;
};

class JsonPatchStep final : public PostponableJsonStep {
public:
  explicit JsonPatchStep(jsoncons::json patch, bool postponable = true)
      : PostponableJsonStep(postponable), patch_(std::move(patch)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  jsoncons::json patch_;
};

class MergePatchStep final : public PostponableJsonStep {
public:
  explicit MergePatchStep(jsoncons::json patch, bool postponable = true)
      : PostponableJsonStep(postponable), patch_(std::move(patch)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  jsoncons::json patch_;
};

class JsonSchemaStep final : public PostponableJsonStep {
public:
  explicit JsonSchemaStep(jsoncons::json schema, bool strict = true,
                          std::string_view error_key = "_schema_errors",
                          bool postponable = true)
      : PostponableJsonStep(postponable),
        schema_(std::move(schema)),
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
