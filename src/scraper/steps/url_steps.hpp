#pragma once

#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <absl/container/flat_hash_set.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scraper {

class ResolveUrlStep final : public Step {
public:
  explicit ResolveUrlStep(std::string_view base_url) : base_url_(base_url) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string base_url_;
};

class UrlPartStep final : public Step {
public:
  enum class Part { Scheme, Host, Port, Path, Query, Fragment };

  explicit UrlPartStep(std::string_view part);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  Part part_;
};

class QueryParamStep final : public Step {
public:
  QueryParamStep(std::string_view name, std::string_view default_val = "")
      : param_name_(name), default_val_(default_val) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string param_name_;
  std::string default_val_;
};

class QueryParamsStep final : public Step {
public:
  using ParamMapping = std::pair<std::string, std::string>;

  explicit QueryParamsStep(std::vector<ParamMapping> param_to_key,
                           std::string_view default_val = "",
                           InsertOptions opts = {})
      : param_to_key_(std::move(param_to_key)), default_val_(default_val),
        opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<ParamMapping> param_to_key_;
  std::string default_val_;
  InsertOptions opts_;
};

class StripQueryStep final : public Step {
public:
  StripQueryStep(const std::vector<std::string> &keep_params = {},
                 const std::vector<std::string> &drop_params = {})
      : keep_params_(keep_params.begin(), keep_params.end()),
        drop_params_(drop_params.begin(), drop_params.end()) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<std::string> keep_params_;
  std::vector<std::string> drop_params_;
};

class FilterUrlsStep final : public Step {
public:
  FilterUrlsStep(
      std::vector<std::string> allowed_domains = {},
      std::vector<std::string> blocked_domains = {},
      std::vector<std::string> allowed_schemes = {"http", "https"},
      std::string_view same_origin_of = "");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<std::string> allowed_domains_;
  std::vector<std::string> blocked_domains_;
  absl::flat_hash_set<std::string> allowed_schemes_;
  std::string same_origin_scheme_;
  std::string same_origin_host_;
  int same_origin_port_ = 0;
  bool check_same_origin_ = false;
};

class SlugifyStep final : public Step {
public:
  SlugifyStep(std::string_view separator = "-",
              std::optional<size_t> max_len = std::nullopt)
      : separator_(separator), max_len_(max_len) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string separator_;
  std::optional<size_t> max_len_;
};

} // namespace scraper
