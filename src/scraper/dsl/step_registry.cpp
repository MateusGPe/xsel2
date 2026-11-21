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
          {"to_number", "parse_number"},
          {"url_join", "resolve_url"},
          {"inner_html", "html"},
          {"fields", "extract_fields"},
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
         const auto &val = require_arg(args, 0, "attr_name", name);
         if (val.is_list()) {
           b.AttrFallback(as_str_list(val));
         } else {
           b.Attr(as_str(val));
         }
       }},
      {"find_all",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         const auto &type = require_arg(args, 1, "type", name);
         b.FindAll(as_str(sel), parse_value_type(as_str(type)),
                   opt_str(args, 2, "attr", ""),
                   opt_bool(args, 3, "optional", false));
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
                opt_non_negative_size(args, 4, "nth_val", name, 0),
                opt_bool(args, 5, "optional", false));
       }},
      {"try_find",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         const auto &type = require_arg(args, 1, "type", name);
         const auto *num = find_arg(args, 3, "number");
         b.Find(as_str(sel), parse_value_type(as_str(type)),
                opt_str(args, 2, "attr", ""),
                num ? parse_number_type(as_str(*num))
                    : QueryStep::Number::All,
                opt_non_negative_size(args, 4, "nth_val", name, 0), true);
       }},
      {"find_first",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &sel = require_arg(args, 0, "selector", name);
         const auto *type_arg = find_arg(args, 1, "type");
         const auto type = (type_arg && !type_arg->is_null())
                               ? parse_value_type(as_str(*type_arg))
                               : QueryAllStep::Type::Text;
         std::vector<std::string> attrs;
         if (const auto *av = find_arg(args, 2, "attr"); av && !av->is_null()) {
           attrs = as_str_list(*av);
         }
         b.FindFirst(as_str(sel), type, std::move(attrs),
                     opt_bool(args, 3, "squish", true),
                     opt_bool(args, 4, "required", false));
       }},
      {"extract_fields",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &spec_val = require_arg(args, 0, "spec", name);
         if (!spec_val.is_object()) {
           throw std::runtime_error(
               "extract_fields: spec must be an object {key: selector_or_spec}");
         }
         std::vector<FieldSpec> specs;
         specs.reserve(spec_val.get_object().size());
         for (const auto &[k, val] : spec_val.get_object()) {
           FieldSpec fs;
           fs.key = k;
           if (!val.is_object()) {
             fs.selector_str = as_str(val);
             specs.push_back(std::move(fs));
             continue;
           }
           bool has_sel = false;
           bool has_type = false;
           bool has_attr = false;
           for (const auto &[fk, fv] : val.get_object()) {
             if (fk == "sel" || fk == "selector") {
               fs.selector_str = as_str(fv);
               has_sel = true;
             } else if (fk == "type") {
               fs.type = parse_value_type(as_str(fv));
               has_type = true;
             } else if (fk == "attr") {
               if (!fv.is_null()) {
                 fs.attrs = as_str_list(fv);
                 has_attr = true;
               }
             } else if (fk == "squish") {
               fs.squish = as_bool(fv);
             } else if (fk == "trim") {
               fs.trim = as_bool(fv);
             } else if (fk == "first") {
               fs.first_only = as_bool(fv);
             } else if (fk == "cast" || fk == "as" || fk == "to") {
               fs.cast = parse_value_type_name(as_str(fv));
               fs.cast_explicit = true;
             } else if (fk == "default") {
               fs.default_val = dsl_value_to_json(fv);
             } else if (fk == "join") {
               if (!fv.is_null()) {
                 fs.join_delim = as_str(fv);
               }
             } else {
               throw std::runtime_error(
                   "extract_fields: unknown field option '" + fk + "' for '" +
                   k + "'");
             }
           }
           if (!has_sel) {
             throw std::runtime_error(
                 "extract_fields: missing 'sel' in field spec for '" + k + "'");
           }
           if (!has_type && has_attr) {
             fs.type = QueryAllStep::Type::Attribute;
           }
           specs.push_back(std::move(fs));
         }
         InsertOptions opts;
         opts.allow_empty = opt_bool(args, 1, "allow_empty", false);
         b.ExtractFields(std::move(specs), opts);
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
      {"regex_find_all",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &pat = require_arg(args, 0, "pattern", name);
         b.RegexFindAll(as_str(pat), opt_int(args, 1, "group_index", 0),
                        opt_flags(args, 2));
       }},
      {"regex_groups",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &pat = require_arg(args, 0, "pattern", name);
         b.RegexGroups(as_str(pat), opt_flags(args, 1),
                       opt_str(args, 2, "prefix", ""),
                       opt_bool(args, 3, "clear", false));
       }},
      {"filter",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Filter(as_str_list(require_arg(args, 0, "patterns", name)));
       }},
      {"set",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const std::string key = as_str(require_arg(args, 0, "name", name));
         const std::string type_str = opt_str(args, 1, "type", "string");
         b.Set(key, InsertOptions{}, parse_value_type_name(type_str));
       }},
      {"append_to",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.AppendTo(as_str(require_arg(args, 0, "key", name)),
                    opt_bool(args, 1, "allow_empty", false));
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
         const auto &first = require_arg(args, 0, "prefixes", name);
         std::vector<std::pair<std::string, std::string>> entries;
         if (first.is_object()) {
           entries.reserve(first.get_object().size());
           for (const auto &[k, v] : first.get_object()) {
             entries.emplace_back(k, as_str(v));
           }
         } else {
           for (const auto &p : as_str_list(first)) {
             if (p.size() > 1 && p.back() == '*') {
               std::string prefix = p.substr(0, p.size() - 1);
               std::string key = prefix;
               if (key.size() > 1 && key.back() == '_') {
                 key.pop_back();
               }
               entries.emplace_back(std::move(prefix), std::move(key));
             } else {
               entries.emplace_back(p, p);
             }
           }
         }
         if (const DslValue *al = find_arg(args, 1, "aliases");
             al && !al->is_null()) {
           if (!al->is_object()) {
             throw std::runtime_error(
                 "js_blocks: aliases must be an object {prefix: target_key}");
           }
           for (const auto &[k, v] : al->get_object()) {
             entries.emplace_back(k, as_str(v));
           }
         }
         b.JsBlocksAliased(std::move(entries));
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
                     opt_bool(args, 6, "strict", true),
                     scraper::InsertOptions{},
                     opt_bool(args, 7, "lenient", false));
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
      {"parse_number",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ParseNumber(opt_str(args, 0, "decimal", "."),
                       opt_str(args, 1, "thousands", ","),
                       opt_bool(args, 2, "allow_float", true),
                       opt_str(args, 3, "key", ""), scraper::InsertOptions{},
                       opt_bool(args, 4, "compact", false));
       }},
      {"parse_duration",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ParseDuration(opt_str(args, 0, "key", ""),
                         opt_str(args, 1, "unit", "seconds"));
       }},
      {"to_int",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         std::optional<jsoncons::json> def;
         if (const DslValue *dv = find_arg(args, 1, "default_val");
             dv && !dv->is_null()) {
           def = dsl_value_to_json(*dv);
         }
         b.ToInt(opt_str(args, 0, "key", ""), std::move(def));
       }},
      {"to_float",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         std::optional<jsoncons::json> def;
         if (const DslValue *dv = find_arg(args, 1, "default_val");
             dv && !dv->is_null()) {
           def = dsl_value_to_json(*dv);
         }
         b.ToFloat(opt_str(args, 0, "key", ""), std::move(def));
       }},
      {"to_bool",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         std::optional<jsoncons::json> def;
         if (const DslValue *dv = find_arg(args, 1, "default_val");
             dv && !dv->is_null()) {
           def = dsl_value_to_json(*dv);
         }
         b.ToBool(opt_str(args, 0, "key", ""), std::move(def));
       }},
      {"cast_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &obj = require_arg(args, 0, "schema", name);
         const jsoncons::json j = as_json_document(obj);
         if (!j.is_object()) {
           throw std::runtime_error("cast_keys: schema must be an object");
         }
         absl::flat_hash_map<std::string, ValueType> schema;
         for (const auto &kv : j.object_range()) {
           if (!kv.value().is_string()) {
             throw std::runtime_error(
                 "cast_keys: schema values must be type name strings");
           }
           schema.emplace(kv.key(), parse_value_type_name(kv.value().as_string_view()));
         }
         b.CastKeys(std::move(schema), opt_bool(args, 1, "postponable", true));
       }},
      {"resolve_url",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.ResolveUrl(as_str(require_arg(args, 0, "base_url", name)));
       }},
      {"url_part",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.UrlPart(as_str(require_arg(args, 0, "part", name)));
       }},
      {"query_param",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.QueryParam(as_str(require_arg(args, 0, "name", name)),
                      opt_str(args, 1, "default_val", ""));
       }},
      {"strip_query",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         std::vector<std::string> keep;
         std::vector<std::string> drop;
         if (const DslValue *kp = find_arg(args, 0, "keep_params");
             kp && !kp->is_null()) {
           keep = as_str_list(*kp);
         }
         if (const DslValue *dp = find_arg(args, 1, "drop_params");
             dp && !dp->is_null()) {
           drop = as_str_list(*dp);
         }
         b.StripQuery(keep, drop);
       }},
      {"filter_urls",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         std::vector<std::string> allowed_domains;
         std::vector<std::string> blocked_domains;
         std::vector<std::string> allowed_schemes = {"http", "https"};
         if (const DslValue *ad = find_arg(args, 0, "allowed_domains");
             ad && !ad->is_null()) {
           allowed_domains = as_str_list(*ad);
         }
         if (const DslValue *bd = find_arg(args, 1, "blocked_domains");
             bd && !bd->is_null()) {
           blocked_domains = as_str_list(*bd);
         }
         if (const DslValue *as = find_arg(args, 2, "allowed_schemes");
             as && !as->is_null()) {
           allowed_schemes = as_str_list(*as);
         }
         b.FilterUrls(std::move(allowed_domains), std::move(blocked_domains),
                      std::move(allowed_schemes),
                      opt_str(args, 3, "same_origin_of", ""));
       }},
      {"slugify",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         std::optional<size_t> max_len;
         if (const DslValue *ml = find_arg(args, 1, "max_len");
             ml && !ml->is_null()) {
           max_len = opt_non_negative_size(args, 1, "max_len", name, 0);
         }
         b.Slugify(opt_str(args, 0, "separator", "-"), max_len);
       }},
      {"html",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Html(opt_bool(args, 0, "outer", false));
       }},
      {"outer_html",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.Html(true);
       }},
      {"tag_name",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.TagName();
       }},
      {"classes",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.Classes();
       }},
      {"attrs",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Attrs(opt_bool(args, 0, "data_only", false));
       }},
      {"strip_tags",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.StripTags();
       }},
      {"base64_encode",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Base64Encode(opt_bool(args, 0, "urlsafe", false));
       }},
      {"base64_decode",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Base64Decode(opt_bool(args, 0, "urlsafe", false));
       }},
      {"hash",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Hash(opt_str(args, 0, "algo", "fnv1a64"));
       }},
      {"replace_many",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const jsoncons::json j =
             as_json_document(require_arg(args, 0, "mapping", name));
         if (!j.is_object()) {
           throw std::runtime_error("replace_many: mapping must be an object");
         }
         absl::flat_hash_map<std::string, std::string> mapping;
         for (const auto &kv : j.object_range()) {
           if (!kv.value().is_string()) {
             throw std::runtime_error(
                 "replace_many: mapping values must be strings");
           }
           mapping.emplace(kv.key(), kv.value().as_string());
         }
         b.ReplaceMany(mapping);
       }},
      {"extract_many",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.ExtractMany(as_str_list(require_arg(args, 0, "patterns", name)),
                       opt_bool(args, 1, "unique", true));
       }},
      {"sort",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Sort(opt_bool(args, 0, "ascending", true),
                opt_bool(args, 1, "numeric", false));
       }},
      {"reverse",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.Reverse();
       }},
      {"coalesce",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Coalesce(opt_str(args, 0, "fallback", ""));
       }},
      {"zip_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.ZipKeys(as_str(require_arg(args, 0, "keys_key", name)),
                   as_str(require_arg(args, 1, "values_key", name)),
                   opt_str(args, 2, "target_key", ""),
                   opt_bool(args, 3, "postponable", true));
       }},
      {"xpath",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.XPath(as_str(require_arg(args, 0, "expr", name)),
                 opt_str(args, 1, "attr", ""));
       }},
      {"table",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.Table(opt_str(args, 0, "key", ""), opt_bool(args, 1, "header", true),
                 opt_bool(args, 2, "normalize_headers", false),
                 opt_bool(args, 3, "trim_cells", true));
       }},
      {"extract_meta",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ExtractMeta(opt_str(args, 0, "key", "meta"),
                       opt_bool(args, 1, "include_json_ld", true),
                       scraper::InsertOptions{},
                       opt_bool(args, 2, "index_json_ld", false));
       }},
      {"parse_date",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ParseDate(opt_str(args, 0, "fmt", ""),
                     opt_str(args, 1, "output", "iso"),
                     opt_str(args, 2, "key", ""), opt_str(args, 3, "tz", "UTC"),
                     scraper::InsertOptions{}, opt_str(args, 4, "now", ""));
       }},
      {"unicode_normalize",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.UnicodeNormalize(opt_str(args, 0, "form", "NFC"));
       }},
      {"strip_accents",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.StripAccents();
       }},
      {"fuzzy_filter",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         double threshold = 0.8;
         if (const DslValue *th = find_arg(args, 1, "threshold");
             th && !th->is_null()) {
           if (th->is_double()) {
             threshold = std::get<double>(th->value);
           } else if (th->is_int()) {
             threshold = static_cast<double>(std::get<int>(th->value));
           } else {
             throw std::runtime_error(
                 "fuzzy_filter: threshold must be a number");
           }
         }
         b.FuzzyFilter(as_str(require_arg(args, 0, "query", name)), threshold,
                       opt_str(args, 2, "algo", "jaro_winkler"),
                       opt_bool(args, 3, "case_sensitive", false),
                       opt_bool(args, 4, "sort_by_score", false));
       }},
      {"similarity",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Similarity(as_str(require_arg(args, 0, "query", name)),
                      opt_str(args, 1, "algo", "jaro_winkler"),
                      opt_bool(args, 2, "case_sensitive", false),
                      opt_str(args, 3, "key", ""));
       }},
      {"map_values",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const jsoncons::json j =
             as_json_document(require_arg(args, 0, "mapping", name));
         if (!j.is_object()) {
           throw std::runtime_error("map_values: mapping must be an object");
         }
         absl::flat_hash_map<std::string, std::string> mapping;
         mapping.reserve(j.size());
         for (const auto &kv : j.object_range()) {
           mapping.emplace(kv.key(), xsel::common::json_to_string(kv.value()));
         }
         std::optional<std::string> def;
         if (const DslValue *dv = find_arg(args, 1, "default_val");
             dv && !dv->is_null()) {
           def = as_str(*dv);
         }
         b.MapValues(std::move(mapping), std::move(def));
       }},
      {"save",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Save(as_str(require_arg(args, 0, "name", name)));
       }},
      {"restore",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.Restore(as_str(require_arg(args, 0, "name", name)));
       }},
      {"coalesce_keys",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &mapping = require_arg(args, 0, "mapping", name);
         if (!mapping.is_object()) {
           throw std::runtime_error(
               "coalesce_keys: mapping must be an object {target: [src, ...]}");
         }
         std::vector<CoalesceKeysStep::Rule> rules;
         rules.reserve(mapping.get_object().size());
         for (const auto &[target, srcs] : mapping.get_object()) {
           rules.emplace_back(target, as_str_list(srcs));
         }
         b.CoalesceKeys(std::move(rules),
                        opt_bool(args, 1, "drop_sources", true),
                        opt_bool(args, 2, "postponable", true));
       }},
      {"query_params",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         const auto &mapping = require_arg(args, 0, "mapping", name);
         std::vector<QueryParamsStep::ParamMapping> pairs;
         if (mapping.is_object()) {
           pairs.reserve(mapping.get_object().size());
           for (const auto &[param_name, target_key] : mapping.get_object()) {
             pairs.emplace_back(param_name, as_str(target_key));
           }
         } else {
           for (const auto &p : as_str_list(mapping)) {
             pairs.emplace_back(p, p);
           }
         }
         b.QueryParams(std::move(pairs), opt_str(args, 1, "default_val", ""),
                       opt_bool(args, 2, "clear", false));
       }},
      {"skip_if_exists",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.SkipIfExists(as_str(require_arg(args, 0, "selector", name)));
       }},
      {"skip_if_empty",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.SkipIfEmpty();
       }},
      {"abort_if_exists",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view name) {
         b.AbortIfExists(as_str(require_arg(args, 0, "selector", name)),
                         opt_str(args, 1, "key", ""),
                         opt_str(args, 2, "reason", ""));
       }},
      {"unescape_html",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.UnescapeHtml();
       }},
      {"parse_srcset",
       [](ScraperBuilder &b, const DslArgList &args, std::string_view) {
         b.ParseSrcset(opt_str(args, 0, "mode", "largest"));
       }},
      {"css_url",
       [](ScraperBuilder &b, const DslArgList &, std::string_view) {
         b.CssUrl();
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
  validate_args_consumed(args, name);
}

} // namespace scraper::dsl
