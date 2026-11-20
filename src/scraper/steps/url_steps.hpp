#pragma once

#include "scraper/step.hpp"
#include <absl/container/flat_hash_set.h>
#include <optional>
#include <string>
#include <string_view>
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

class StripQueryStep final : public Step {
public:
  explicit StripQueryStep(const std::vector<std::string> &keep_params = {})
      : keep_params_(keep_params.begin(), keep_params.end()) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  absl::flat_hash_set<std::string> keep_params_;
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
