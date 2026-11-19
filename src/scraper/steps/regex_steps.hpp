#pragma once

#include "common/ahocorasick.hpp"
#include "scraper/step.hpp"
#include <optional>
#include <re2/re2.h>
#include <string>
#include <string_view>
#include <vector>

namespace scraper {

namespace detail {
re2::RE2::Options get_re2_options(const std::optional<std::string> &flags_opt);
} // namespace detail

class RegexReplaceStep final : public Step {
public:
  RegexReplaceStep(std::string_view pattern, std::string_view replacement,
                   const std::optional<std::string> &flags = std::nullopt);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string pattern_str_;
  re2::RE2 regex_;
  std::string replacement_str_;
};

class RegexFilterStep final : public Step {
public:
  RegexFilterStep(std::string_view pattern, bool negate = false,
                  const std::optional<std::string> &flags = std::nullopt);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string pattern_str_;
  re2::RE2 regex_;
  bool negate_match_;
};

class RegexExtractStep final : public Step {
public:
  RegexExtractStep(std::string_view pattern, int group_index = 0,
                   const std::optional<std::string> &flags = std::nullopt);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string pattern_str_;
  re2::RE2 regex_;
  int group_index_;
};

class FilterStep final : public Step {
public:
  explicit FilterStep(const std::vector<std::string> &patterns);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  AhoCorasick include_ac_;
  AhoCorasick exclude_ac_;
  bool has_include_{false};
  bool has_exclude_{false};
};

} // namespace scraper
