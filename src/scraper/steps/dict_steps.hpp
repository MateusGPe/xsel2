#pragma once

#include "query/json_query.hpp"
#include "scraper/step.hpp"
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scraper {

class FlattenDictStep final : public JsonTransformStep {
protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  [[nodiscard]] jsoncons::json flatten_impl(const jsoncons::json &input,
                                            int depth) const;
};

class FilterDictStep final : public JsonTransformStep {
public:
  explicit FilterDictStep(const std::vector<std::string> &keys_to_keep)
      : keys_to_keep_(keys_to_keep.begin(), keys_to_keep.end()) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_set<std::string> keys_to_keep_;
};

class RemoveKeysStep final : public JsonTransformStep {
public:
  explicit RemoveKeysStep(const std::vector<std::string> &keys_to_remove)
      : keys_to_remove_(keys_to_remove.begin(), keys_to_remove.end()) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_set<std::string> keys_to_remove_;
  [[nodiscard]] jsoncons::json remove_impl(const jsoncons::json &input,
                                           int depth) const;
};

class RenameKeysStep final : public JsonTransformStep {
public:
  explicit RenameKeysStep(
      absl::flat_hash_map<std::string, std::string> rename_map)
      : rename_map_(std::move(rename_map)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_map<std::string, std::string> rename_map_;
  [[nodiscard]] jsoncons::json rename_impl(const jsoncons::json &input,
                                           int depth) const;
};

class MergeKeysStep final : public JsonTransformStep {
public:
  MergeKeysStep(std::vector<std::string> source_keys, std::string target_key)
      : source_keys_(std::move(source_keys)),
        target_key_(std::move(target_key)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::vector<std::string> source_keys_;
  std::string target_key_;
};

class SortArrayStep final : public JsonTransformStep {
public:
  explicit SortArrayStep(bool ascending = true) noexcept
      : ascending_(ascending) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  bool ascending_;
  [[nodiscard]] jsoncons::json sort_impl(const jsoncons::json &input,
                                         int depth) const;
};

class DeduplicateStep final : public JsonTransformStep {
protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  [[nodiscard]] jsoncons::json dedup_impl(const jsoncons::json &input,
                                          int depth) const;
};

class JsonQueryStep final : public JsonTransformStep {
public:
  JsonQueryStep(xsel::query::QueryDialect dialect, std::string_view expr)
      : dialect_(dialect), expr_(expr) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  xsel::query::QueryDialect dialect_;
  std::string expr_;
};

class ZipKeysStep final : public JsonTransformStep {
public:
  ZipKeysStep(std::string_view keys_key, std::string_view values_key,
              std::string_view target_key = "")
      : keys_key_(keys_key),
        values_key_(values_key),
        target_key_(target_key) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string keys_key_;
  std::string values_key_;
  std::string target_key_;
};

class TeeStep final : public Step {
public:
  explicit TeeStep(std::vector<std::shared_ptr<Step>> steps)
      : steps_(std::move(steps)) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  std::vector<std::shared_ptr<Step>> steps_;
};

class SaveValuesStep final : public Step {
public:
  explicit SaveValuesStep(std::string_view name) : name_(name) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  std::string name_;
};

class RestoreValuesStep final : public Step {
public:
  explicit RestoreValuesStep(std::string_view name) : name_(name) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

  void apply(Node node, std::vector<std::string> &values, jsoncons::json &dict,
             ExecutionContext &ctx) override;

private:
  std::string name_;
};

class CoalesceKeysStep final : public JsonTransformStep {
public:
  using Rule = std::pair<std::string, std::vector<std::string>>;

  explicit CoalesceKeysStep(std::vector<Rule> rules, bool drop_sources = true)
      : rules_(std::move(rules)), drop_sources_(drop_sources) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::vector<Rule> rules_;
  bool drop_sources_;
};

} // namespace scraper
