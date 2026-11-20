#pragma once

#include "query/json_query.hpp"
#include "scraper/step.hpp"
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

class FlattenDictStep final : public PostponableJsonStep {
public:
  explicit FlattenDictStep(bool postponable = true) noexcept
      : PostponableJsonStep(postponable) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  [[nodiscard]] jsoncons::json flatten_impl(const jsoncons::json &input,
                                            int depth) const;
};

class FilterDictStep final : public PostponableJsonStep {
public:
  explicit FilterDictStep(const std::vector<std::string> &keys_to_keep,
                          bool postponable = true)
      : PostponableJsonStep(postponable),
        keys_to_keep_(keys_to_keep.begin(), keys_to_keep.end()) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_set<std::string> keys_to_keep_;
};

class RemoveKeysStep final : public PostponableJsonStep {
public:
  explicit RemoveKeysStep(const std::vector<std::string> &keys_to_remove,
                          bool postponable = true)
      : PostponableJsonStep(postponable),
        keys_to_remove_(keys_to_remove.begin(), keys_to_remove.end()) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_set<std::string> keys_to_remove_;
  [[nodiscard]] jsoncons::json remove_impl(const jsoncons::json &input,
                                           int depth) const;
};

class RenameKeysStep final : public PostponableJsonStep {
public:
  explicit RenameKeysStep(
      absl::flat_hash_map<std::string, std::string> rename_map,
      bool postponable = true)
      : PostponableJsonStep(postponable), rename_map_(std::move(rename_map)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  absl::flat_hash_map<std::string, std::string> rename_map_;
  [[nodiscard]] jsoncons::json rename_impl(const jsoncons::json &input,
                                           int depth) const;
};

class MergeKeysStep final : public PostponableJsonStep {
public:
  MergeKeysStep(std::vector<std::string> source_keys, std::string target_key,
                bool postponable = true)
      : PostponableJsonStep(postponable),
        source_keys_(std::move(source_keys)),
        target_key_(std::move(target_key)) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::vector<std::string> source_keys_;
  std::string target_key_;
};

class SortKeysStep final : public PostponableJsonStep {
public:
  explicit SortKeysStep(bool ascending = true, bool postponable = true) noexcept
      : PostponableJsonStep(postponable), ascending_(ascending) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  bool ascending_;
  [[nodiscard]] jsoncons::json sort_impl(const jsoncons::json &input,
                                         int depth) const;
};

class DeduplicateStep final : public PostponableJsonStep {
public:
  explicit DeduplicateStep(bool postponable = true) noexcept
      : PostponableJsonStep(postponable) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  [[nodiscard]] jsoncons::json dedup_impl(const jsoncons::json &input,
                                          int depth) const;
};

class JsonQueryStep final : public PostponableJsonStep {
public:
  JsonQueryStep(xsel::query::QueryDialect dialect, std::string_view expr,
                bool postponable = true)
      : PostponableJsonStep(postponable), dialect_(dialect), expr_(expr) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  xsel::query::QueryDialect dialect_;
  std::string expr_;
};

class ZipKeysStep final : public PostponableJsonStep {
public:
  ZipKeysStep(std::string_view keys_key, std::string_view values_key,
              std::string_view target_key = "", bool postponable = true)
      : PostponableJsonStep(postponable), keys_key_(keys_key),
        values_key_(values_key), target_key_(target_key) {}

protected:
  [[nodiscard]] jsoncons::json
  transform(const jsoncons::json &input) const override;

private:
  std::string keys_key_;
  std::string values_key_;
  std::string target_key_;
};

} // namespace scraper
