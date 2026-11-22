#pragma once

#include "scraper/extractor.hpp"
#include "scraper/steps/block_steps.hpp"
#include "scraper/steps/dict_steps.hpp"
#include "scraper/steps/dom_steps.hpp"
#include "scraper/steps/json_steps.hpp"
#include "scraper/steps/regex_steps.hpp"
#include "scraper/steps/text_steps.hpp"
#include "scraper/steps/type_steps.hpp"
#include "scraper/steps/url_steps.hpp"
#include <absl/container/flat_hash_map.h>
#include <concepts>
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
  Scraper(const Scraper &other);
  Scraper &operator=(const Scraper &other);
  Scraper(Scraper &&) noexcept = default;
  Scraper &operator=(Scraper &&) noexcept = default;
  virtual ~Scraper() = default;

  void scrape_document(Document &document, jsoncons::json &data,
                       std::string_view error_prefix,
                       std::ostream &error_stream = std::cerr);

  void scrape_document(Document &document, jsoncons::json &data,
                       std::string_view error_prefix,
                       std::ostream &error_stream,
                       const ExtractionOptions &opts);

  void perform_extraction(QueryResult &selection, jsoncons::json &data,
                          std::string_view error_prefix,
                          std::ostream &error_stream = std::cerr);

  void perform_extraction(QueryResult &selection, jsoncons::json &data,
                          std::string_view error_prefix,
                          std::ostream &error_stream,
                          const ExtractionOptions &opts);

  [[nodiscard]] jsoncons::json scrape_from_string(std::string_view content);
  [[nodiscard]] jsoncons::json scrape_from_file(std::string_view filename);

  void set_error_policy(ErrorPolicy policy) noexcept { error_policy_ = policy; }
  [[nodiscard]] ErrorPolicy error_policy() const noexcept {
    return error_policy_;
  }
  [[nodiscard]] const std::vector<std::string> &errors() const noexcept {
    return errors_;
  }
  void clear_errors() noexcept { errors_.clear(); }

  [[nodiscard]] const std::vector<std::shared_ptr<Extractor>> &
  get_extractors() const noexcept {
    return extractors_;
  }

  void set_extractors(std::vector<std::shared_ptr<Extractor>> extractors) {
    extractors_ = std::move(extractors);
  }

  [[nodiscard]] const std::vector<std::shared_ptr<Step>> &
  get_finalize_steps() const noexcept {
    return finalize_steps_;
  }

  void set_finalize_steps(std::vector<std::shared_ptr<Step>> steps) {
    finalize_steps_ = std::move(steps);
  }

  void set_in_block(bool v) noexcept { in_block_ = v; }
  [[nodiscard]] bool in_block() const noexcept { return in_block_; }

protected:
  void add_step(std::shared_ptr<Step> step);
  void add_dict_step(std::shared_ptr<Step> step);
  std::vector<std::shared_ptr<Extractor>> extractors_;
  std::vector<std::shared_ptr<Step>> finalize_steps_;
  ErrorPolicy error_policy_ = ErrorPolicy::LogAndContinue;
  std::vector<std::string> errors_;
  bool in_block_ = false;
};

template <typename Derived> class ScraperBuilderBase : public Scraper {
public:
  Derived &Select(std::string_view selector, std::string_view key = "") {
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

  Derived &Squish() {
    add_step(std::make_shared<StripStep>());
    return self();
  }

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

  Derived &Replace(std::string_view from, std::string_view to = "",
                   bool first_only = false) {
    add_step(std::make_shared<ReplaceStep>(from, to, first_only));
    return self();
  }

  Derived &
  Replace(const absl::flat_hash_map<std::string, std::string> &mapping) {
    add_step(std::make_shared<ReplaceManyStep>(mapping));
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

  Derived &DropEmpty(bool also_trim = true) {
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

  Derived &
  RegexFindAll(std::string_view pattern, int group_index = 0,
               const std::optional<std::string> &flags = std::nullopt) {
    add_step(std::make_shared<RegexFindAllStep>(pattern, group_index, flags));
    return self();
  }

  Derived &
  RegexGroups(std::string_view pattern,
              const std::optional<std::string> &flags = std::nullopt,
              std::string_view prefix = "", InsertOptions opts = {}) {
    add_step(std::make_shared<RegexGroupsStep>(pattern, flags, prefix, opts));
    return self();
  }

  Derived &Filter(const std::vector<std::string> &patterns) {
    add_step(std::make_shared<FilterStep>(patterns));
    return self();
  }

  Derived &Find(std::string_view sel,
                QueryAllStep::Type type = QueryAllStep::Type::Text,
                std::vector<std::string> attrs = {}, bool squish = true,
                bool required = false) {
    add_step(std::make_shared<FindFirstStep>(sel, type, std::move(attrs),
                                             squish, required));
    return self();
  }

  template <std::convertible_to<std::string_view> AttrStr>
  Derived &Find(std::string_view sel, QueryAllStep::Type type, AttrStr &&attr,
                bool squish = true, bool required = false) {
    add_step(std::make_shared<FindFirstStep>(
        sel, type, std::forward<AttrStr>(attr), squish, required));
    return self();
  }

  Derived &FindAll(std::string_view sel,
                   QueryAllStep::Type type = QueryAllStep::Type::Text,
                   std::vector<std::string> attrs = {}, bool squish = false,
                   bool required = false) {
    add_step(std::make_shared<QueryAllStep>(sel, type, std::move(attrs), squish,
                                            required));
    return self();
  }

  template <std::convertible_to<std::string_view> AttrStr>
  Derived &FindAll(std::string_view sel, QueryAllStep::Type type,
                   AttrStr &&attr, bool squish = false, bool required = false) {
    add_step(std::make_shared<QueryAllStep>(
        sel, type, std::forward<AttrStr>(attr), squish, required));
    return self();
  }

  Derived &ExtractFields(std::vector<FieldSpec> specs,
                         InsertOptions opts = {}) {
    add_step(std::make_shared<ExtractFieldsStep>(std::move(specs), opts));
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

  Derived &AttrFallback(std::vector<std::string> attr_names) {
    add_step(std::make_shared<AttrStep>(std::move(attr_names)));
    return self();
  }

  Derived &Set(std::string_view name, InsertOptions opts = {},
               ValueType type = ValueType::String) {
    add_step(std::make_shared<SetStep>(name, opts, type));
    return self();
  }

  Derived &AppendTo(std::string_view key, bool allow_empty = false) {
    add_step(std::make_shared<AppendToStep>(key, allow_empty));
    return self();
  }

  Derived &Count(std::string_view dict_key, InsertOptions opts = {}) {
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

  Derived &Slice(size_t start_idx, size_t end_idx_inclusive) {
    add_step(std::make_shared<SliceSelectionStep>(
        SliceSelectionStep::Mode::Range, start_idx, end_idx_inclusive));
    return self();
  }

  Derived &Unique(bool preserve_order = true) {
    add_step(std::make_shared<UniqueStep>(preserve_order));
    return self();
  }

  Derived &Default(std::string_view default_value) {
    add_step(std::make_shared<DefaultIfEmptyStep>(default_value));
    return self();
  }

  Derived &Debug(std::string_view prefix = "DEBUG: ", bool print_dict = false,
                 bool print_node = false, std::ostream &out = std::cerr) {
    add_step(std::make_shared<DebugPrintStep>(prefix, print_dict, print_node,
                                              out));
    return self();
  }

  Derived &JsBlock(std::string_view prefix = "") {
    add_step(std::make_shared<BlockSeekerStep>(prefix));
    return self();
  }

  Derived &JsBlocks(std::vector<std::string> prefixes) {
    add_step(std::make_shared<BlockParserStep>(std::move(prefixes)));
    return self();
  }

  Derived &
  JsBlocksAliased(std::vector<std::pair<std::string, std::string>> entries) {
    add_step(std::make_shared<BlockParserStep>(BlockParserStep::AliasedTag{},
                                               std::move(entries)));
    return self();
  }

  Derived &ParseJs(std::string_view objname, bool simplify = true,
                   InsertOptions opts = {}) {
    add_step(std::make_shared<ParseStep>(objname, simplify, opts));
    return self();
  }

  Derived &Flatten() {
    add_dict_step(std::make_shared<FlattenDictStep>());
    return self();
  }

  Derived &PickKeys(const std::vector<std::string> &keys_to_keep) {
    add_dict_step(std::make_shared<FilterDictStep>(keys_to_keep));
    return self();
  }

  Derived &OmitKeys(const std::vector<std::string> &keys_to_remove) {
    add_dict_step(std::make_shared<RemoveKeysStep>(keys_to_remove));
    return self();
  }

  Derived &
  RenameKeys(absl::flat_hash_map<std::string, std::string> rename_map) {
    add_dict_step(std::make_shared<RenameKeysStep>(std::move(rename_map)));
    return self();
  }

  Derived &MergeKeys(std::vector<std::string> source_keys,
                     std::string target_key) {
    add_dict_step(std::make_shared<MergeKeysStep>(std::move(source_keys),
                                                  std::move(target_key)));
    return self();
  }

  Derived &SortArray(bool ascending = true) {
    add_dict_step(std::make_shared<SortArrayStep>(ascending));
    return self();
  }

  Derived &Deduplicate() {
    add_dict_step(std::make_shared<DeduplicateStep>());
    return self();
  }

  Derived &JmesPath(std::string_view expr) {
    add_dict_step(std::make_shared<JsonQueryStep>(
        xsel::query::QueryDialect::JmesPath, expr));
    return self();
  }

  Derived &JsonPath(std::string_view expr) {
    add_dict_step(std::make_shared<JsonQueryStep>(
        xsel::query::QueryDialect::JsonPath, expr));
    return self();
  }

  Derived &JsonParse(std::string_view objname = "",
                     xsel::query::JsonParseOptions opts = {},
                     bool strict = true, InsertOptions insert_opts = {},
                     bool lenient = false) {
    add_step(std::make_shared<JsonParseStep>(objname, opts, strict, insert_opts,
                                             lenient));
    return self();
  }

  Derived &JsonDump(bool pretty = false, int indent = 2,
                    std::string_view source_key = "") {
    add_step(std::make_shared<JsonDumpStep>(pretty, indent, source_key));
    return self();
  }

  Derived &CsvParse(std::string_view objname = "",
                    xsel::query::CsvOptions opts = {}, bool strict = true,
                    InsertOptions insert_opts = {}) {
    add_step(std::make_shared<CsvParseStep>(objname, opts, strict, insert_opts));
    return self();
  }

  Derived &CsvDump(xsel::query::CsvOptions opts = {},
                   std::string_view source_key = "") {
    add_step(std::make_shared<CsvDumpStep>(opts, source_key));
    return self();
  }

  Derived &JsonSchema(jsoncons::json schema, bool strict = true,
                      std::string_view error_key = "_schema_errors") {
    add_dict_step(std::make_shared<JsonSchemaStep>(std::move(schema), strict,
                                                   error_key));
    return self();
  }

  // ── Tier 1: Typed parsing, URLs, DOM gaps, Aho-Corasick, Hash/Base64 ──────

  Derived &ParseNumber(std::string_view decimal = ".",
                       std::string_view thousands = ",",
                       bool allow_float = true, std::string_view key = "",
                       InsertOptions opts = {}, bool compact = false) {
    add_step(std::make_shared<ParseNumberStep>(decimal, thousands, allow_float,
                                               key, opts, compact));
    return self();
  }

  Derived &ParseDuration(std::string_view key = "",
                         std::string_view unit = "seconds",
                         InsertOptions opts = {}) {
    add_step(std::make_shared<ParseDurationStep>(key, unit, opts));
    return self();
  }

  Derived &ToInt(std::string_view key = "",
                 std::optional<jsoncons::json> default_val = std::nullopt,
                 InsertOptions opts = {}) {
    add_step(std::make_shared<CastValuesStep>(ValueType::Int, key,
                                              std::move(default_val), opts));
    return self();
  }

  Derived &ToFloat(std::string_view key = "",
                   std::optional<jsoncons::json> default_val = std::nullopt,
                   InsertOptions opts = {}) {
    add_step(std::make_shared<CastValuesStep>(ValueType::Float, key,
                                              std::move(default_val), opts));
    return self();
  }

  Derived &ToBool(std::string_view key = "",
                  std::optional<jsoncons::json> default_val = std::nullopt,
                  InsertOptions opts = {}) {
    add_step(std::make_shared<CastValuesStep>(ValueType::Bool, key,
                                              std::move(default_val), opts));
    return self();
  }

  Derived &CastKeys(absl::flat_hash_map<std::string, ValueType> schema) {
    add_dict_step(std::make_shared<CastKeysStep>(std::move(schema)));
    return self();
  }

  Derived &ResolveUrl(std::string_view base_url) {
    add_step(std::make_shared<ResolveUrlStep>(base_url));
    return self();
  }

  Derived &UrlPart(std::string_view part) {
    add_step(std::make_shared<UrlPartStep>(part));
    return self();
  }

  Derived &QueryParam(std::string_view name,
                      std::string_view default_val = "") {
    add_step(std::make_shared<QueryParamStep>(name, default_val));
    return self();
  }

  Derived &StripQuery(const std::vector<std::string> &keep_params = {},
                      const std::vector<std::string> &drop_params = {}) {
    add_step(std::make_shared<StripQueryStep>(keep_params, drop_params));
    return self();
  }

  Derived &FilterUrls(
      std::vector<std::string> allowed_domains = {},
      std::vector<std::string> blocked_domains = {},
      std::vector<std::string> allowed_schemes = {"http", "https"},
      std::string_view same_origin_of = "") {
    add_step(std::make_shared<FilterUrlsStep>(
        std::move(allowed_domains), std::move(blocked_domains),
        std::move(allowed_schemes), same_origin_of));
    return self();
  }

  Derived &Slugify(std::string_view separator = "-",
                   std::optional<size_t> max_len = std::nullopt) {
    add_step(std::make_shared<SlugifyStep>(separator, max_len));
    return self();
  }

  Derived &Html(bool outer = false) {
    add_step(std::make_shared<HtmlStep>(outer));
    return self();
  }

  Derived &TagName() {
    add_step(std::make_shared<TagNameStep>());
    return self();
  }

  Derived &Classes() {
    add_step(std::make_shared<ClassesStep>());
    return self();
  }

  Derived &Attrs(bool data_only = false, InsertOptions opts = {}) {
    add_step(std::make_shared<AttrsStep>(data_only, opts));
    return self();
  }

  Derived &StripTags() {
    add_step(std::make_shared<StripTagsStep>());
    return self();
  }

  Derived &Base64Encode(bool urlsafe = false) {
    add_step(std::make_shared<Base64EncodeStep>(urlsafe));
    return self();
  }

  Derived &Base64Decode(bool urlsafe = false) {
    add_step(std::make_shared<Base64DecodeStep>(urlsafe));
    return self();
  }

  Derived &Hash(std::string_view algo = "fnv1a64") {
    add_step(std::make_shared<HashStep>(algo));
    return self();
  }

  Derived &ExtractMany(const std::vector<std::string> &patterns,
                       bool unique = true) {
    add_step(std::make_shared<ExtractManyStep>(patterns, unique));
    return self();
  }

  Derived &Sort(bool ascending = true, bool numeric = false) {
    add_step(std::make_shared<SortValuesStep>(ascending, numeric));
    return self();
  }

  Derived &Reverse() {
    add_step(std::make_shared<ReverseValuesStep>());
    return self();
  }

  Derived &ZipKeys(std::string_view keys_key, std::string_view values_key,
                   std::string_view target_key = "") {
    add_dict_step(
        std::make_shared<ZipKeysStep>(keys_key, values_key, target_key));
    return self();
  }

  Derived &XPath(std::string_view expr, std::string_view attr = "") {
    add_step(std::make_shared<XPathStep>(expr, attr));
    return self();
  }

  Derived &Table(std::string_view key = "", bool header = true,
                 bool normalize_headers = false, bool trim_cells = true,
                 InsertOptions opts = {}) {
    add_step(std::make_shared<TableStep>(key, header, normalize_headers,
                                         trim_cells, opts));
    return self();
  }

  Derived &ExtractMeta(std::string_view key = "meta",
                       bool include_json_ld = true, InsertOptions opts = {},
                       bool index_json_ld = false) {
    add_step(std::make_shared<ExtractMetaStep>(key, include_json_ld, opts,
                                               index_json_ld));
    return self();
  }

  Derived &ParseDate(std::string_view fmt = "", std::string_view output = "iso",
                     std::string_view key = "", std::string_view tz = "UTC",
                     InsertOptions opts = {}, std::string_view now = "") {
    add_step(std::make_shared<ParseDateStep>(fmt, output, key, tz, opts, now));
    return self();
  }

  Derived &UnicodeNormalize(std::string_view form = "NFC") {
    add_step(std::make_shared<UnicodeNormalizeStep>(form));
    return self();
  }

  Derived &StripAccents() {
    add_step(std::make_shared<StripAccentsStep>());
    return self();
  }

  Derived &FuzzyFilter(std::string_view query, double threshold = 0.8,
                       std::string_view algo = "jaro_winkler",
                       bool case_sensitive = false, bool sort_by_score = false) {
    add_step(std::make_shared<FuzzyFilterStep>(query, threshold, algo,
                                               case_sensitive, sort_by_score));
    return self();
  }

  Derived &Similarity(std::string_view query,
                      std::string_view algo = "jaro_winkler",
                      bool case_sensitive = false, std::string_view key = "") {
    add_step(std::make_shared<SimilarityStep>(query, algo, case_sensitive, key));
    return self();
  }

  Derived &MapValues(absl::flat_hash_map<std::string, std::string> mapping,
                     std::optional<std::string> default_val = std::nullopt) {
    add_step(std::make_shared<MapValuesStep>(std::move(mapping),
                                             std::move(default_val)));
    return self();
  }

  Derived &UnescapeHtml() {
    add_step(std::make_shared<UnescapeHtmlStep>());
    return self();
  }

  Derived &ParseSrcset(std::string_view mode = "largest") {
    add_step(std::make_shared<ParseSrcsetStep>(mode));
    return self();
  }

  Derived &CssUrl() {
    add_step(std::make_shared<CssUrlStep>());
    return self();
  }

  Derived &Tee(std::vector<std::shared_ptr<Step>> steps) {
    add_step(std::make_shared<TeeStep>(std::move(steps)));
    return self();
  }

  Derived &Save(std::string_view name) {
    add_step(std::make_shared<SaveValuesStep>(name));
    return self();
  }

  Derived &Restore(std::string_view name) {
    add_step(std::make_shared<RestoreValuesStep>(name));
    return self();
  }

  Derived &CoalesceKeys(std::vector<CoalesceKeysStep::Rule> rules,
                        bool drop_sources = true) {
    add_dict_step(
        std::make_shared<CoalesceKeysStep>(std::move(rules), drop_sources));
    return self();
  }

  Derived &QueryParams(std::vector<QueryParamsStep::ParamMapping> param_to_key,
                       std::string_view default_val = "",
                       InsertOptions opts = {}) {
    add_step(std::make_shared<QueryParamsStep>(std::move(param_to_key),
                                               default_val, opts));
    return self();
  }

  Derived &AddStep(std::shared_ptr<Step> step) {
    add_step(std::move(step));
    return self();
  }

  Derived &Each(std::string_view selector, std::string_view key,
                std::vector<std::shared_ptr<Step>> steps) {
    SelectorPtr sel_ptr;
    if (!selector.empty()) {
      sel_ptr = SelectorParser::Create(std::string(selector));
      if (!sel_ptr) {
        throw std::runtime_error("each: failed to parse CSS selector: " +
                                 std::string(selector));
      }
    }
    auto ext = std::make_shared<Extractor>(std::move(sel_ptr), key);
    for (auto &step : steps) {
      ext->append_step(std::move(step));
    }
    add_step(std::make_shared<SubExtractStep>(std::move(ext)));
    return self();
  }

  Derived &WhenExists(std::string_view selector,
                      std::vector<std::shared_ptr<Step>> steps,
                      bool negate = false) {
    add_step(ConditionalStep::make_exists(selector, negate, std::move(steps)));
    return self();
  }

  Derived &UnlessExists(std::string_view selector,
                        std::vector<std::shared_ptr<Step>> steps) {
    return WhenExists(selector, std::move(steps), true);
  }

  Derived &WhenMatch(std::string_view pattern,
                     std::vector<std::shared_ptr<Step>> steps,
                     bool negate = false,
                     const std::optional<std::string> &flags = std::nullopt) {
    add_step(ConditionalStep::make_match(pattern, negate, flags,
                                         std::move(steps)));
    return self();
  }

  Derived &SkipIfExists(std::string_view selector) {
    add_step(GuardStep::make_skip_if_exists(selector));
    return self();
  }

  Derived &SkipIfEmpty() {
    add_step(GuardStep::make_skip_if_empty());
    return self();
  }

  Derived &AbortIfExists(std::string_view selector, std::string_view key = "",
                         std::string_view reason = "") {
    add_step(GuardStep::make_abort_if_exists(selector, key, reason));
    return self();
  }

  Derived &Finalize(std::vector<std::shared_ptr<Step>> steps) {
    for (auto &step : steps) {
      if (step) {
        finalize_steps_.push_back(std::move(step));
      }
    }
    return self();
  }

  Derived &Include(const Scraper &other) {
    for (const auto &ext : other.get_extractors()) {
      if (ext) {
        extractors_.push_back(std::make_shared<Extractor>(*ext));
      }
    }
    for (const auto &fstep : other.get_finalize_steps()) {
      if (fstep) {
        finalize_steps_.push_back(fstep);
      }
    }
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
