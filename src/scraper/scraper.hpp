#pragma once

#include "scraper/extractor.hpp"
#include "scraper/steps/block_steps.hpp"
#include "scraper/steps/dict_steps.hpp"
#include "scraper/steps/dom_steps.hpp"
#include "scraper/steps/regex_steps.hpp"
#include "scraper/steps/text_steps.hpp"
#include <absl/container/flat_hash_map.h>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scraper {

class Scraper {
public:
  Scraper() = default;
  Scraper(const Scraper &) = default;
  Scraper &operator=(const Scraper &) = default;
  Scraper(Scraper &&) noexcept = default;
  Scraper &operator=(Scraper &&) noexcept = default;
  virtual ~Scraper() = default;

  void scrape_document(Document &document, jsoncons::json &data,
                       std::string_view error_prefix,
                       std::ostream &error_stream = std::cerr);

  void perform_extraction(QueryResult &selection, jsoncons::json &data,
                          std::string_view error_prefix,
                          std::ostream &error_stream = std::cerr);

  [[nodiscard]] jsoncons::json scrape_from_string(std::string_view content);
  [[nodiscard]] jsoncons::json scrape_from_file(std::string_view filename);

  [[nodiscard]] const std::vector<std::shared_ptr<Extractor>> &
  get_extractors() const noexcept {
    return extractors_;
  }

  void set_extractors(std::vector<std::shared_ptr<Extractor>> extractors) {
    extractors_ = std::move(extractors);
  }

protected:
  void add_step(std::shared_ptr<Step> step);
  std::vector<std::shared_ptr<Extractor>> extractors_;
};

template <typename Derived> class ScraperBuilderBase : public Scraper {
public:
  Derived &Group(std::string_view selector, std::string_view key = "") {
    if (selector.empty()) {
      extractors_.push_back(std::make_shared<Extractor>(nullptr, key));
    } else {
      SelectorPtr sel_ptr = SelectorParser::Create(std::string(selector));
      if (!sel_ptr) {
        throw std::runtime_error("Failed to parse CSS selector: " +
                                 std::string(selector));
      }
      extractors_.push_back(
          std::make_shared<Extractor>(std::move(sel_ptr), key));
    }
    return self();
  }

  Derived &Trim() {
    add_step(std::make_shared<TrimStep>());
    return self();
  }

  Derived &Normspace() {
    add_step(std::make_shared<StripStep>());
    return self();
  }

  Derived &Strip() { return Normspace(); }

  Derived &Lower() {
    add_step(std::make_shared<LowerStep>());
    return self();
  }

  Derived &Upper() {
    add_step(std::make_shared<UpperStep>());
    return self();
  }

  Derived &Unescape(bool url_decode = true) {
    add_step(std::make_shared<UnescapeStep>(url_decode));
    return self();
  }

  Derived &Replace(std::string_view from, std::string_view to = "") {
    add_step(std::make_shared<ReplaceStep>(from, to));
    return self();
  }

  Derived &Substitute(std::string_view from, std::string_view to = "") {
    add_step(std::make_shared<SubstituteStep>(from, to));
    return self();
  }

  Derived &Split(std::string_view delimiter, bool keep_empty_parts = false) {
    add_step(std::make_shared<SplitStep>(delimiter, keep_empty_parts));
    return self();
  }

  Derived &Join(std::string_view delimiter) {
    add_step(std::make_shared<JoinStep>(delimiter));
    return self();
  }

  Derived &Prepend(std::string_view prefix) {
    add_step(std::make_shared<PrependStep>(prefix));
    return self();
  }

  Derived &Append(std::string_view suffix) {
    add_step(std::make_shared<AppendStep>(suffix));
    return self();
  }

  Derived &RemoveEmpty(bool also_trim = true) {
    add_step(std::make_shared<RemoveEmptyStep>(also_trim));
    return self();
  }

  Derived &Substring(int start_pos,
                     std::optional<int> length = std::nullopt) {
    add_step(std::make_shared<SubstringStep>(start_pos, length));
    return self();
  }

  Derived &StripComments(bool multiline = true, bool single_line = true) {
    add_step(std::make_shared<StripCommentsStep>(multiline, single_line));
    return self();
  }

  Derived &RegexReplace(std::string_view pattern, std::string_view replacement,
                        const std::optional<std::string> &flags = std::nullopt) {
    add_step(std::make_shared<RegexReplaceStep>(pattern, replacement, flags));
    return self();
  }

  Derived &RegexFilter(std::string_view pattern, bool negate = false,
                       const std::optional<std::string> &flags = std::nullopt) {
    add_step(std::make_shared<RegexFilterStep>(pattern, negate, flags));
    return self();
  }

  Derived &RegexExtract(std::string_view pattern, int group_index = 0,
                        const std::optional<std::string> &flags = std::nullopt) {
    add_step(std::make_shared<RegexExtractStep>(pattern, group_index, flags));
    return self();
  }

  Derived &Filter(const std::vector<std::string> &patterns) {
    add_step(std::make_shared<FilterStep>(patterns));
    return self();
  }

  Derived &QueryAll(std::string_view sel, QueryAllStep::Type type,
                    std::string_view attr = "") {
    add_step(std::make_shared<QueryAllStep>(sel, type, attr));
    return self();
  }

  Derived &Query(std::string_view sel, QueryAllStep::Type type,
                 std::string_view attr = "",
                 QueryStep::Number number = QueryStep::Number::All,
                 size_t nth_val = 0) {
    add_step(std::make_shared<QueryStep>(sel, type, attr, number, nth_val));
    return self();
  }

  Derived &Text(bool own_text = false) {
    add_step(std::make_shared<TextStep>(own_text));
    return self();
  }

  Derived &Attr(std::string_view attr_name) {
    add_step(std::make_shared<AttrStep>(attr_name));
    return self();
  }

  Derived &Set(std::string_view name, InsertOptions opts = {}) {
    add_step(std::make_shared<SetStep>(name, opts));
    return self();
  }

  Derived &CountValues(std::string_view dict_key, InsertOptions opts = {}) {
    add_step(std::make_shared<CountValuesStep>(dict_key, opts));
    return self();
  }

  Derived &First(size_t count = 1) {
    add_step(std::make_shared<SliceSelectionStep>(
        SliceSelectionStep::Mode::First, count));
    return self();
  }

  Derived &Last(size_t count = 1) {
    add_step(std::make_shared<SliceSelectionStep>(
        SliceSelectionStep::Mode::Last, count));
    return self();
  }

  Derived &Nth(size_t index) {
    add_step(std::make_shared<SliceSelectionStep>(
        SliceSelectionStep::Mode::Nth, index));
    return self();
  }

  Derived &SliceValues(size_t start_idx, size_t end_idx_inclusive) {
    add_step(std::make_shared<SliceSelectionStep>(
        SliceSelectionStep::Mode::Range, start_idx, end_idx_inclusive));
    return self();
  }

  Derived &Unique(bool preserve_order = true) {
    add_step(std::make_shared<UniqueStep>(preserve_order));
    return self();
  }

  Derived &DefaultIfEmpty(std::string_view default_value) {
    add_step(std::make_shared<DefaultIfEmptyStep>(default_value));
    return self();
  }

  Derived &DebugPrint(std::string_view prefix = "DEBUG: ",
                      bool print_dict = false, bool print_node = false,
                      std::ostream &out = std::cerr) {
    add_step(std::make_shared<DebugPrintStep>(prefix, print_dict, print_node,
                                              out));
    return self();
  }

  Derived &BlockSeeker(std::string_view prefix = "") {
    add_step(std::make_shared<BlockSeekerStep>(prefix));
    return self();
  }

  Derived &BlockParser(std::vector<std::string> prefixes) {
    add_step(std::make_shared<BlockParserStep>(std::move(prefixes)));
    return self();
  }

  Derived &Parse(std::string_view objname, bool simplify = true,
                 InsertOptions opts = {}) {
    add_step(std::make_shared<ParseStep>(objname, simplify, opts));
    return self();
  }

  Derived &FlattenDict(bool postponable = true) {
    add_step(std::make_shared<FlattenDictStep>(postponable));
    return self();
  }

  Derived &FlattenLocal() { return FlattenDict(false); }

  Derived &FilterDict(const std::vector<std::string> &keys_to_keep,
                      bool postponable = true) {
    add_step(std::make_shared<FilterDictStep>(keys_to_keep, postponable));
    return self();
  }

  Derived &RemoveKeys(const std::vector<std::string> &keys_to_remove,
                      bool postponable = true) {
    add_step(std::make_shared<RemoveKeysStep>(keys_to_remove, postponable));
    return self();
  }

  Derived &RenameKeys(absl::flat_hash_map<std::string, std::string> rename_map,
                      bool postponable = true) {
    add_step(
        std::make_shared<RenameKeysStep>(std::move(rename_map), postponable));
    return self();
  }

  Derived &MergeKeys(std::vector<std::string> source_keys,
                     std::string target_key, bool postponable = true) {
    add_step(std::make_shared<MergeKeysStep>(
        std::move(source_keys), std::move(target_key), postponable));
    return self();
  }

  Derived &SortKeys(bool ascending = true, bool postponable = true) {
    add_step(std::make_shared<SortKeysStep>(ascending, postponable));
    return self();
  }

  Derived &Deduplicate(bool postponable = true) {
    add_step(std::make_shared<DeduplicateStep>(postponable));
    return self();
  }

  Derived &JmesPath(std::string_view expr, bool postponable = true) {
    add_step(std::make_shared<JsonQueryStep>(
        xsel::query::QueryDialect::JmesPath, expr, postponable));
    return self();
  }

  Derived &JsonPath(std::string_view expr, bool postponable = true) {
    add_step(std::make_shared<JsonQueryStep>(
        xsel::query::QueryDialect::JsonPath, expr, postponable));
    return self();
  }

private:
  Derived &self() { return static_cast<Derived &>(*this); }
};

class ScraperBuilder final : public ScraperBuilderBase<ScraperBuilder> {
public:
  using ScraperBuilderBase<ScraperBuilder>::ScraperBuilderBase;
};

} // namespace scraper
