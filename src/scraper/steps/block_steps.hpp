#pragma once

#include "scraper/merge.hpp"
#include "scraper/step.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scraper {

namespace detail {
[[nodiscard]] std::string block_seeker_extract(std::string_view content,
                                               std::string_view prefix);
} // namespace detail

class BlockSeekerStep final : public Step {
public:
  explicit BlockSeekerStep(std::string_view prefix = "")
      : prefix_str_(prefix) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string prefix_str_;
};

class ParseStep final : public Step {
public:
  ParseStep(std::string_view objname, bool simplify = true,
            InsertOptions opts = {})
      : objname_(objname), simplify_(simplify), opts_(opts) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string objname_;
  bool simplify_;
  InsertOptions opts_;
};

class BlockParserStep final : public Step {
public:
  struct AliasedTag {};

  explicit BlockParserStep(std::vector<std::string> prefixes) {
    entries_.reserve(prefixes.size());
    for (auto &p : prefixes) {
      std::string key = p;
      entries_.emplace_back(std::move(p), std::move(key));
    }
  }

  BlockParserStep(AliasedTag,
                  std::vector<std::pair<std::string, std::string>> entries)
      : entries_(std::move(entries)) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::vector<std::pair<std::string, std::string>> entries_;
};

} // namespace scraper
