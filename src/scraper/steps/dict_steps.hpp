#pragma once

#include "query/json_query.hpp"
#include "scraper/step.hpp"
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

class FlattenDictStep final : public Step {
public:
  explicit FlattenDictStep(bool postponable = true)
      : postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  bool postponable_;
  [[nodiscard]] jsoncons::json flatten_impl(const jsoncons::json &input) const;
};

class FilterDictStep final : public Step {
public:
  explicit FilterDictStep(const std::vector<std::string> &keys_to_keep,
                          bool postponable = true)
      : keys_to_keep_(keys_to_keep.begin(), keys_to_keep.end()),
        postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  absl::flat_hash_set<std::string> keys_to_keep_;
  bool postponable_;
  [[nodiscard]] jsoncons::json filter_impl(const jsoncons::json &input) const;
};

class RemoveKeysStep final : public Step {
public:
  explicit RemoveKeysStep(const std::vector<std::string> &keys_to_remove,
                          bool postponable = true)
      : keys_to_remove_(keys_to_remove.begin(), keys_to_remove.end()),
        postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  absl::flat_hash_set<std::string> keys_to_remove_;
  bool postponable_;
  [[nodiscard]] jsoncons::json remove_impl(const jsoncons::json &input,
                                           int depth = 0) const;
};

class RenameKeysStep final : public Step {
public:
  explicit RenameKeysStep(
      absl::flat_hash_map<std::string, std::string> rename_map,
      bool postponable = true)
      : rename_map_(std::move(rename_map)), postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  absl::flat_hash_map<std::string, std::string> rename_map_;
  bool postponable_;
  [[nodiscard]] jsoncons::json rename_impl(const jsoncons::json &input,
                                           int depth = 0) const;
};

class MergeKeysStep final : public Step {
public:
  MergeKeysStep(std::vector<std::string> source_keys, std::string target_key,
                bool postponable = true)
      : source_keys_(std::move(source_keys)),
        target_key_(std::move(target_key)),
        postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  std::vector<std::string> source_keys_;
  std::string target_key_;
  bool postponable_;
  [[nodiscard]] jsoncons::json merge_impl(const jsoncons::json &input) const;
};

class SortKeysStep final : public Step {
public:
  explicit SortKeysStep(bool ascending = true, bool postponable = true)
      : ascending_(ascending), postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  bool ascending_;
  bool postponable_;
  [[nodiscard]] jsoncons::json sort_impl(const jsoncons::json &input,
                                         int depth = 0) const;
};

class DeduplicateStep final : public Step {
public:
  explicit DeduplicateStep(bool postponable = true)
      : postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  bool postponable_;
  [[nodiscard]] jsoncons::json dedup_impl(const jsoncons::json &input,
                                          int depth = 0) const;
};

class JsonQueryStep final : public Step {
public:
  JsonQueryStep(xsel::query::QueryDialect dialect, std::string_view expr,
                bool postponable = true)
      : dialect_(dialect), expr_(expr), postponable_(postponable) {}

  [[nodiscard]] bool is_postponable() const noexcept override {
    return postponable_;
  }
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const override;

private:
  xsel::query::QueryDialect dialect_;
  std::string expr_;
  bool postponable_;
};

} // namespace scraper
