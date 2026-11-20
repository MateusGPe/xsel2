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

protected:
  void add_step(std::shared_ptr<Step> step);
  std::vector<std::shared_ptr<Extractor>> extractors_;
  ErrorPolicy error_policy_ = ErrorPolicy::LogAndContinue;
  std::vector<std::string> errors_;
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

  Derived &Replace(std::string_view from, std::string_view to = "") {
    add_step(std::make_shared<ReplaceStep>(from, to));
    return self();
  }

  Derived &ReplaceExact(std::string_view from, std::string_view to = "") {
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

  Derived &Filter(const std::vector<std::string> &patterns) {
    add_step(std::make_shared<FilterStep>(patterns));
    return self();
  }

  Derived &FindAll(std::string_view sel, QueryAllStep::Type type,
                   std::string_view attr = "") {
    add_step(std::make_shared<QueryAllStep>(sel, type, attr));
    return self();
  }

  Derived &Find(std::string_view sel, QueryAllStep::Type type,
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

  Derived &ParseJs(std::string_view objname, bool simplify = true,
                   InsertOptions opts = {}) {
    add_step(std::make_shared<ParseStep>(objname, simplify, opts));
    return self();
  }

  Derived &Flatten(bool postponable = true) {
    add_step(std::make_shared<FlattenDictStep>(postponable));
    return self();
  }

  Derived &FlattenLocal() { return Flatten(false); }

  Derived &PickKeys(const std::vector<std::string> &keys_to_keep,
                    bool postponable = true) {
    add_step(std::make_shared<FilterDictStep>(keys_to_keep, postponable));
    return self();
  }

  Derived &OmitKeys(const std::vector<std::string> &keys_to_remove,
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

  Derived &FromCbor(
      std::string_view objname = "",
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      bool strict = true, InsertOptions insert_opts = {}) {
    add_step(std::make_shared<BinaryDecodeStep>(
        xsel::query::BinaryFormat::Cbor, objname, enc, strict, insert_opts));
    return self();
  }

  Derived &ToCbor(
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      std::string_view source_key = "") {
    add_step(std::make_shared<BinaryEncodeStep>(
        xsel::query::BinaryFormat::Cbor, enc, source_key));
    return self();
  }

  Derived &FromMsgPack(
      std::string_view objname = "",
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      bool strict = true, InsertOptions insert_opts = {}) {
    add_step(std::make_shared<BinaryDecodeStep>(
        xsel::query::BinaryFormat::MsgPack, objname, enc, strict, insert_opts));
    return self();
  }

  Derived &ToMsgPack(
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      std::string_view source_key = "") {
    add_step(std::make_shared<BinaryEncodeStep>(
        xsel::query::BinaryFormat::MsgPack, enc, source_key));
    return self();
  }

  Derived &FromBson(
      std::string_view objname = "",
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      bool strict = true, InsertOptions insert_opts = {}) {
    add_step(std::make_shared<BinaryDecodeStep>(
        xsel::query::BinaryFormat::Bson, objname, enc, strict, insert_opts));
    return self();
  }

  Derived &ToBson(
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      std::string_view source_key = "") {
    add_step(std::make_shared<BinaryEncodeStep>(
        xsel::query::BinaryFormat::Bson, enc, source_key));
    return self();
  }

  Derived &FromUbjson(
      std::string_view objname = "",
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      bool strict = true, InsertOptions insert_opts = {}) {
    add_step(std::make_shared<BinaryDecodeStep>(
        xsel::query::BinaryFormat::Ubjson, objname, enc, strict, insert_opts));
    return self();
  }

  Derived &ToUbjson(
      xsel::query::BinaryEncoding enc = xsel::query::BinaryEncoding::Hex,
      std::string_view source_key = "") {
    add_step(std::make_shared<BinaryEncodeStep>(
        xsel::query::BinaryFormat::Ubjson, enc, source_key));
    return self();
  }

  Derived &JsonPathReplace(std::string_view expr, jsoncons::json replacement,
                           bool postponable = true) {
    add_step(std::make_shared<JsonPathReplaceStep>(expr, std::move(replacement),
                                                   postponable));
    return self();
  }

  Derived &JsonFlatten(bool postponable = true) {
    add_step(std::make_shared<JsonFlattenStep>(postponable));
    return self();
  }

  Derived &JsonUnflatten(bool postponable = true) {
    add_step(std::make_shared<JsonUnflattenStep>(postponable));
    return self();
  }

  Derived &JsonPointerGet(std::string_view pointer,
                          std::string_view target_key = "",
                          bool postponable = true) {
    add_step(std::make_shared<JsonPointerGetStep>(pointer, target_key,
                                                  postponable));
    return self();
  }

  Derived &JsonPointerSet(std::string_view pointer, jsoncons::json value,
                          bool postponable = true) {
    add_step(std::make_shared<JsonPointerSetStep>(pointer, std::move(value),
                                                  postponable));
    return self();
  }

  Derived &JsonPointerRemove(std::string_view pointer,
                             bool postponable = true) {
    add_step(std::make_shared<JsonPointerRemoveStep>(pointer, postponable));
    return self();
  }

  Derived &JsonPatch(jsoncons::json patch, bool postponable = true) {
    add_step(std::make_shared<JsonPatchStep>(std::move(patch), postponable));
    return self();
  }

  Derived &MergePatch(jsoncons::json patch, bool postponable = true) {
    add_step(std::make_shared<MergePatchStep>(std::move(patch), postponable));
    return self();
  }

  Derived &JsonSchema(jsoncons::json schema, bool strict = true,
                      std::string_view error_key = "_schema_errors",
                      bool postponable = true) {
    add_step(std::make_shared<JsonSchemaStep>(std::move(schema), strict,
                                              error_key, postponable));
    return self();
  }

  // ── Tier 1: Typed parsing, URLs, DOM gaps, Aho-Corasick, Hash/Base64 ──────

  Derived &ParseNumber(std::string_view decimal = ".",
                       std::string_view thousands = ",",
                       bool allow_float = true, std::string_view key = "",
                       InsertOptions opts = {}) {
    add_step(std::make_shared<ParseNumberStep>(decimal, thousands, allow_float,
                                               key, opts));
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

  Derived &CastKeys(absl::flat_hash_map<std::string, ValueType> schema,
                    bool postponable = true) {
    add_step(std::make_shared<CastKeysStep>(std::move(schema), postponable));
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

  Derived &StripQuery(const std::vector<std::string> &keep_params = {}) {
    add_step(std::make_shared<StripQueryStep>(keep_params));
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

  Derived &ReplaceMany(
      const absl::flat_hash_map<std::string, std::string> &mapping) {
    add_step(std::make_shared<ReplaceManyStep>(mapping));
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

  Derived &Coalesce(std::string_view fallback = "") {
    add_step(std::make_shared<CoalesceStep>(fallback));
    return self();
  }

  Derived &ZipKeys(std::string_view keys_key, std::string_view values_key,
                   std::string_view target_key = "", bool postponable = true) {
    add_step(std::make_shared<ZipKeysStep>(keys_key, values_key, target_key,
                                           postponable));
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
                       bool include_json_ld = true, InsertOptions opts = {}) {
    add_step(std::make_shared<ExtractMetaStep>(key, include_json_ld, opts));
    return self();
  }

  Derived &ParseDate(std::string_view fmt = "", std::string_view output = "iso",
                     std::string_view key = "", std::string_view tz = "UTC",
                     InsertOptions opts = {}) {
    add_step(std::make_shared<ParseDateStep>(fmt, output, key, tz, opts));
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

private:
  Derived &self() { return static_cast<Derived &>(*this); }
};

class ScraperBuilder final : public ScraperBuilderBase<ScraperBuilder> {
public:
  using ScraperBuilderBase<ScraperBuilder>::ScraperBuilderBase;
};

} // namespace scraper
