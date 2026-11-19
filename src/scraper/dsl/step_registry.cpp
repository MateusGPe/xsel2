#include "scraper/dsl/step_registry.hpp"
#include <absl/container/flat_hash_map.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace scraper::dsl {

namespace {

QueryAllStep::Type parse_value_type(const std::string &s) {
  if (s == "Value.Text" || s == "Text" || s == "text") {
    return QueryAllStep::Type::Text;
  }
  if (s == "Value.OwnText" || s == "OwnText" || s == "own_text") {
    return QueryAllStep::Type::OwnText;
  }
  if (s == "Value.Attribute" || s == "Attribute" || s == "attr") {
    return QueryAllStep::Type::Attribute;
  }
  if (s == "Value.ElementExists" || s == "ElementExists" || s == "exists") {
    return QueryAllStep::Type::ElementExists;
  }
  if (s == "Value.AttributeExists" || s == "AttributeExists" ||
      s == "attr_exists") {
    return QueryAllStep::Type::AttributeExists;
  }
  if (s == "Value.Adjacent" || s == "Adjacent" || s == "adjacent") {
    return QueryAllStep::Type::AdjacentText;
  }
  throw std::runtime_error("Unknown Value enum in DSL: " + s);
}

QueryStep::Number parse_number_type(const std::string &s) {
  if (s == "Number.All" || s == "All" || s == "all") {
    return QueryStep::Number::All;
  }
  if (s == "Number.Last" || s == "Last" || s == "last") {
    return QueryStep::Number::Last;
  }
  if (s == "Number.Nth" || s == "Nth" || s == "nth") {
    return QueryStep::Number::Nth;
  }
  throw std::runtime_error("Unknown Number enum in DSL: " + s);
}

xsel::query::BinaryEncoding parse_binary_encoding(const std::string &s) {
  if (s == "hex" || s == "Hex") {
    return xsel::query::BinaryEncoding::Hex;
  }
  if (s == "base64" || s == "Base64") {
    return xsel::query::BinaryEncoding::Base64;
  }
  if (s == "raw" || s == "Raw") {
    return xsel::query::BinaryEncoding::Raw;
  }
  throw std::runtime_error("Unknown binary encoding in DSL: " + s +
                           " (expected 'hex', 'base64', or 'raw')");
}

std::optional<std::string> opt_flags(const DslArgList &args, size_t idx) {
  if (const DslValue *fv = find_arg(args, idx, "flags"); fv && !fv->is_null()) {
    return as_str(*fv);
  }
  return std::nullopt;
}

xsel::query::JsonParseOptions parse_json_opts(const DslArgList &args,
                                              size_t start_idx) {
  xsel::query::JsonParseOptions opts;
  opts.allow_comments = opt_bool(args, start_idx, "allow_comments", false);
  opts.allow_trailing_comma =
      opt_bool(args, start_idx + 1, "allow_trailing_comma", false);
  opts.allow_inf_and_nan =
      opt_bool(args, start_idx + 2, "allow_inf_and_nan", false);
  opts.lossless_number =
      opt_bool(args, start_idx + 3, "lossless_number", false);
  opts.max_nesting_depth =
      opt_int(args, start_idx + 4, "max_nesting_depth", 1024);
  return opts;
}

xsel::query::CsvOptions parse_csv_opts(const DslArgList &args,
                                       size_t start_idx) {
  xsel::query::CsvOptions opts;
  opts.assume_header = opt_bool(args, start_idx, "assume_header", true);
  const std::string delim = opt_str(args, start_idx + 1, "delimiter", ",");
  opts.delimiter = delim.empty() ? ',' : delim[0];
  opts.trim = opt_bool(args, start_idx + 2, "trim", true);
  return opts;
}

using MethodHandler = void (*)(ScraperBuilder &, const DslArgList &,
                               std::string_view invoked_name);

const absl::flat_hash_map<std::string_view, std::string_view> &
get_alias_map() {
  static const absl::flat_hash_map<std::string_view, std::string_view> aliases =
      {
          {"strip", "trim"},
          {"clean", "squish"},
          {"normalize_whitespace", "squish"},
          {"sub", "regex_replace"},
          {"grep", "regex_filter"},
          {"extract", "regex_extract"},
          {"as", "set"},
          {"limit", "first"},
          {"dedup", "unique"},
          {"extract_js_block", "js_block"},
          {"extract_js_blocks", "js_blocks"},
          {"json", "json_parse"},
      };
  return aliases;
}

const absl::flat_hash_map<std::string_view, MethodHandler> &
get_method_registry() {
  static const absl::flat_hash_map<std::string_view, MethodHandler> registry = {
      {"select",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         b.Select(as_str(sel), opt_str(args, 1, "key", ""));
       }},
      {"text",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Text(opt_bool(args, 0, "own_text", false));
       }},
      {"attr",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Attr(as_str(require_arg(args, 0, "attr_name", name)));
       }},
      {"find_all",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         const auto &type = require_arg(args, 1, "type", name);
         b.FindAll(as_str(sel), parse_value_type(as_str(type)),
                   opt_str(args, 2, "attr", ""));
       }},
      {"find",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         const auto &type = require_arg(args, 1, "type", name);
         const auto *num = find_arg(args, 3, "number");
         b.Find(as_str(sel), parse_value_type(as_str(type)),
                opt_str(args, 2, "attr", ""),
                num ? parse_number_type(as_str(*num))
                    : QueryStep::Number::All,
                opt_non_negative_size(args, 4, "nth_val", name, 0));
       }},
      {"trim", [](ScraperBuilder &b, const DslArgList &, std::string_view) {
        b.Trim();
      }},
      {"squish", [](ScraperBuilder &b, const DslArgList &, std::string_view) {
        b.Squish();
      }},
      {"lower", [](ScraperBuilder &b, const DslArgList &, std::string_view) {
        b.Lower();
      }},
      {"upper", [](ScraperBuilder &b, const DslArgList &, std::string_view) {
        b.Upper();
      }},
      {"unescape",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Unescape(opt_bool(args, 0, "url_decode", true));
       }},
      {"replace",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Replace(as_str(require_arg(args, 0, "from", name)),
                   opt_str(args, 1, "to", ""));
       }},
      {"replace_exact",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.ReplaceExact(as_str(require_arg(args, 0, "from", name)),
                        opt_str(args, 1, "to", ""));
       }},
      {"split",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Split(as_str(require_arg(args, 0, "delimiter", name)),
                 opt_bool(args, 1, "keep_empty_parts", false));
       }},
      {"join",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Join(as_str(require_arg(args, 0, "delimiter", name)));
       }},
      {"prepend",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Prepend(as_str(require_arg(args, 0, "prefix", name)));
       }},
      {"append",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Append(as_str(require_arg(args, 0, "suffix", name)));
       }},
      {"drop_empty",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.DropEmpty(opt_bool(args, 0, "also_trim", true));
       }},
      {"substring",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const int start = as_int(require_arg(args, 0, "start_pos", name));
         std::optional<int> len;
         if (const DslValue *lv = find_arg(args, 1, "length");
             lv && !lv->is_null()) {
           len = as_int(*lv);
         }
         b.Substring(start, len);
       }},
      {"strip_comments",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.StripComments(opt_bool(args, 0, "multiline", true),
                         opt_bool(args, 1, "single_line", true));
       }},
      {"regex_replace",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &pat = require_arg(args, 0, "pattern", name);
         const auto &rep = require_arg(args, 1, "replacement", name);
         b.RegexReplace(as_str(pat), as_str(rep), opt_flags(args, 2));
       }},
      {"regex_filter",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &pat = require_arg(args, 0, "pattern", name);
         b.RegexFilter(as_str(pat), opt_bool(args, 1, "negate", false),
                       opt_flags(args, 2));
       }},
      {"regex_extract",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &pat = require_arg(args, 0, "pattern", name);
         b.RegexExtract(as_str(pat), opt_int(args, 1, "group_index", 0),
                        opt_flags(args, 2));
       }},
      {"filter",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Filter(as_str_list(require_arg(args, 0, "patterns", name)));
       }},
      {"set",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Set(as_str(require_arg(args, 0, "name", name)));
       }},
      {"count",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Count(as_str(require_arg(args, 0, "dict_key", name)));
       }},
      {"first",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.First(opt_non_negative_size(args, 0, "count", name, 1));
       }},
      {"last",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Last(opt_non_negative_size(args, 0, "count", name, 1));
       }},
      {"nth",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Nth(as_non_negative_size(require_arg(args, 0, "index", name), name));
       }},
      {"slice",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Slice(
             as_non_negative_size(require_arg(args, 0, "start_index", name),
                                  name),
             as_non_negative_size(
                 require_arg(args, 1, "end_index_inclusive", name), name));
       }},
      {"unique",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Unique(opt_bool(args, 0, "preserve_order", true));
       }},
      {"default",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Default(as_str(require_arg(args, 0, "default_value", name)));
       }},
      {"js_block",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.JsBlock(opt_str(args, 0, "prefix", ""));
       }},
      {"js_blocks",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsBlocks(as_str_list(require_arg(args, 0, "prefixes", name)));
       }},
      {"parse_js",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.ParseJs(as_str(require_arg(args, 0, "objname", name)),
                   opt_bool(args, 1, "simplify", true));
       }},
      {"flatten",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Flatten(opt_bool(args, 0, "postponable", true));
       }},
      {"flatten_local",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.FlattenLocal();
       }},
      {"pick_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.PickKeys(as_str_list(require_arg(args, 0, "keys_to_keep", name)),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"omit_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.OmitKeys(as_str_list(require_arg(args, 0, "keys_to_remove", name)),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"rename_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const DslValue &mapping = require_arg(args, 0, "pairs", name);
         absl::flat_hash_map<std::string, std::string> rename_map;
         if (mapping.is_object()) {
           rename_map.reserve(mapping.get_object().size());
           for (const auto &[k, val] : mapping.get_object()) {
             rename_map.emplace(k, as_str(val));
           }
         } else {
           const auto pairs = as_str_list(mapping);
           if (pairs.size() % 2 != 0) {
             throw std::runtime_error(
                 "rename_keys() requires an object {old: new} or even-length "
                 "list [old, new, ...]");
           }
           rename_map.reserve(pairs.size() / 2);
           for (size_t i = 0; i < pairs.size(); i += 2) {
             rename_map.emplace(pairs[i], pairs[i + 1]);
           }
         }
         b.RenameKeys(std::move(rename_map),
                      opt_bool(args, 1, "postponable", true));
       }},
      {"merge_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.MergeKeys(as_str_list(require_arg(args, 0, "source_keys", name)),
                     as_str(require_arg(args, 1, "target_key", name)),
                     opt_bool(args, 2, "postponable", true));
       }},
      {"sort_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.SortKeys(opt_bool(args, 0, "ascending", true),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"deduplicate",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Deduplicate(opt_bool(args, 0, "postponable", true));
       }},
      {"jmespath",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JmesPath(as_str(require_arg(args, 0, "expr", name)),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"jsonpath",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPath(as_str(require_arg(args, 0, "expr", name)),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"json_parse",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.JsonParse(opt_str(args, 0, "objname", ""), parse_json_opts(args, 1),
                     opt_bool(args, 6, "strict", true));
       }},
      {"json_dump",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.JsonDump(opt_bool(args, 0, "pretty", false),
                    opt_int(args, 1, "indent", 2),
                    opt_str(args, 2, "source_key", ""));
       }},
      {"csv_parse",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.CsvParse(opt_str(args, 0, "objname", ""), parse_csv_opts(args, 1),
                    opt_bool(args, 4, "strict", true));
       }},
      {"csv_dump",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.CsvDump(parse_csv_opts(args, 0), opt_str(args, 3, "source_key", ""));
       }},
      {"from_cbor",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.FromCbor(opt_str(args, 0, "objname", ""),
                    parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
                    opt_bool(args, 2, "strict", true));
       }},
      {"to_cbor",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ToCbor(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                  opt_str(args, 1, "source_key", ""));
       }},
      {"from_msgpack",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.FromMsgPack(
             opt_str(args, 0, "objname", ""),
             parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
             opt_bool(args, 2, "strict", true));
       }},
      {"to_msgpack",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ToMsgPack(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                     opt_str(args, 1, "source_key", ""));
       }},
      {"from_bson",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.FromBson(opt_str(args, 0, "objname", ""),
                    parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
                    opt_bool(args, 2, "strict", true));
       }},
      {"to_bson",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ToBson(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                  opt_str(args, 1, "source_key", ""));
       }},
      {"from_ubjson",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.FromUbjson(
             opt_str(args, 0, "objname", ""),
             parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
             opt_bool(args, 2, "strict", true));
       }},
      {"to_ubjson",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ToUbjson(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                    opt_str(args, 1, "source_key", ""));
       }},
      {"jsonpath_replace",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPathReplace(
             as_str(require_arg(args, 0, "expr", name)),
             dsl_value_to_json(require_arg(args, 1, "replacement", name)),
             opt_bool(args, 2, "postponable", true));
       }},
      {"json_flatten",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.JsonFlatten(opt_bool(args, 0, "postponable", true));
       }},
      {"json_unflatten",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.JsonUnflatten(opt_bool(args, 0, "postponable", true));
       }},
      {"jsonpointer_get",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPointerGet(as_str(require_arg(args, 0, "pointer", name)),
                          opt_str(args, 1, "target_key", ""),
                          opt_bool(args, 2, "postponable", true));
       }},
      {"jsonpointer_set",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPointerSet(
             as_str(require_arg(args, 0, "pointer", name)),
             dsl_value_to_json(require_arg(args, 1, "value", name)),
             opt_bool(args, 2, "postponable", true));
       }},
      {"jsonpointer_remove",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPointerRemove(as_str(require_arg(args, 0, "pointer", name)),
                             opt_bool(args, 1, "postponable", true));
       }},
      {"json_patch",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonPatch(as_json_document(require_arg(args, 0, "patch", name)),
                     opt_bool(args, 1, "postponable", true));
       }},
      {"merge_patch",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.MergePatch(as_json_document(require_arg(args, 0, "patch", name)),
                      opt_bool(args, 1, "postponable", true));
       }},
      {"json_schema",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.JsonSchema(as_json_document(require_arg(args, 0, "schema", name)),
                      opt_bool(args, 1, "strict", true),
                      opt_str(args, 2, "error_key", "_schema_errors"),
                      opt_bool(args, 3, "postponable", true));
       }},
      {"debug",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Debug(opt_str(args, 0, "prefix", "DEBUG: "),
                 opt_bool(args, 1, "print_dict", false),
                 opt_bool(args, 2, "print_node", false));
       }},
  };
  return registry;
}

} // namespace

void apply_dsl_method(ScraperBuilder &builder, std::string_view name,
                      const DslArgList &args) {
  const auto &aliases = get_alias_map();
  const auto alias_it = aliases.find(name);
  const std::string_view canonical =
      (alias_it != aliases.end()) ? alias_it->second : name;

  const auto &reg = get_method_registry();
  const auto it = reg.find(canonical);
  if (it == reg.end()) {
    throw std::runtime_error("Unknown DSL method: " + std::string(name));
  }
  it->second(builder, args, name);
}

} // namespace scraper::dsl
