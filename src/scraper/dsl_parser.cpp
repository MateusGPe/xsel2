#include "scraper/dsl_parser.hpp"
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace scraper::dsl {

namespace {

QueryAllStep::Type parse_value_type(const std::string &s) {
  if (s == "Value.Text" || s == "Text" || s == "text")
    return QueryAllStep::Type::Text;
  if (s == "Value.OwnText" || s == "OwnText" || s == "own_text")
    return QueryAllStep::Type::OwnText;
  if (s == "Value.Attribute" || s == "Attribute" || s == "attr")
    return QueryAllStep::Type::Attribute;
  if (s == "Value.ElementExists" || s == "ElementExists" || s == "exists")
    return QueryAllStep::Type::ElementExists;
  if (s == "Value.AttributeExists" || s == "AttributeExists" ||
      s == "attr_exists")
    return QueryAllStep::Type::AttributeExists;
  if (s == "Value.Adjacent" || s == "Adjacent" || s == "adjacent")
    return QueryAllStep::Type::AdjacentText;
  throw std::runtime_error("Unknown Value enum in DSL: " + s);
}

QueryStep::Number parse_number_type(const std::string &s) {
  if (s == "Number.All" || s == "All" || s == "all")
    return QueryStep::Number::All;
  if (s == "Number.Last" || s == "Last" || s == "last")
    return QueryStep::Number::Last;
  if (s == "Number.Nth" || s == "Nth" || s == "nth")
    return QueryStep::Number::Nth;
  throw std::runtime_error("Unknown Number enum in DSL: " + s);
}

xsel::query::BinaryEncoding parse_binary_encoding(const std::string &s) {
  if (s == "hex" || s == "Hex")
    return xsel::query::BinaryEncoding::Hex;
  if (s == "base64" || s == "Base64")
    return xsel::query::BinaryEncoding::Base64;
  if (s == "raw" || s == "Raw")
    return xsel::query::BinaryEncoding::Raw;
  throw std::runtime_error("Unknown binary encoding in DSL: " + s +
                           " (expected 'hex', 'base64', or 'raw')");
}

std::string as_str(const DslValue &v) {
  if (v.is_string())
    return v.get_string();
  if (v.is_identifier())
    return v.get_identifier();
  if (v.is_int())
    return std::to_string(v.get_int());
  if (v.is_double()) {
    std::ostringstream oss;
    oss << v.get_double();
    return oss.str();
  }
  if (v.is_bool())
    return v.get_bool() ? "true" : "false";
  if (v.is_null())
    return "null";
  throw std::runtime_error("Expected scalar string/identifier in DSL");
}

bool as_bool(const DslValue &v) {
  if (v.is_bool())
    return v.get_bool();
  if (v.is_int())
    return v.get_int() != 0;
  if (v.is_string())
    return v.get_string() == "true";
  throw std::runtime_error("Expected boolean in DSL");
}

int as_int(const DslValue &v) {
  if (v.is_int())
    return v.get_int();
  if (v.is_double())
    return static_cast<int>(v.get_double());
  if (v.is_string())
    return std::stoi(v.get_string());
  throw std::runtime_error("Expected integer in DSL");
}

std::vector<std::string> as_str_list(const DslValue &v) {
  if (!v.is_list()) {
    return {as_str(v)};
  }
  std::vector<std::string> out;
  for (const auto &el : v.get_list()) {
    out.push_back(as_str(el));
  }
  return out;
}

jsoncons::json dsl_value_to_json(const DslValue &v, int depth = 0) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL nesting depth exceeded");
  }
  if (v.is_null())
    return jsoncons::json::null();
  if (v.is_string())
    return jsoncons::json(v.get_string());
  if (v.is_int())
    return jsoncons::json(v.get_int());
  if (v.is_double())
    return jsoncons::json(v.get_double());
  if (v.is_bool())
    return jsoncons::json(v.get_bool());
  if (v.is_identifier())
    return jsoncons::json(v.get_identifier());
  if (v.is_list()) {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &el : v.get_list()) {
      arr.push_back(dsl_value_to_json(el, depth + 1));
    }
    return arr;
  }
  if (v.is_object()) {
    jsoncons::json obj = jsoncons::json::object();
    for (const auto &[k, val] : v.get_object()) {
      obj.insert_or_assign(k, dsl_value_to_json(val, depth + 1));
    }
    return obj;
  }
  return jsoncons::json::null();
}

jsoncons::json as_json_document(const DslValue &v) {
  if (v.is_string()) {
    return xsel::query::parse_json(v.get_string());
  }
  return dsl_value_to_json(v);
}

const DslValue *find_arg(const DslArgList &args, size_t idx,
                         std::string_view name) {
  if (!name.empty()) {
    auto it = args.named.find(name);
    if (it != args.named.end()) {
      return &it->second;
    }
  }
  if (idx < args.positional.size()) {
    return &args.positional[idx];
  }
  return nullptr;
}

const DslValue &require_arg(const DslArgList &args, size_t idx,
                            std::string_view name, std::string_view method) {
  const DslValue *v = find_arg(args, idx, name);
  if (!v) {
    throw std::runtime_error(std::string(method) + "() missing required argument '" +
                             std::string(name) + "'");
  }
  return *v;
}

std::string opt_str(const DslArgList &args, size_t idx, std::string_view name,
                    std::string def = "") {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_str(*v);
  }
  return def;
}

bool opt_bool(const DslArgList &args, size_t idx, std::string_view name,
              bool def = false) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_bool(*v);
  }
  return def;
}

int opt_int(const DslArgList &args, size_t idx, std::string_view name,
            int def = 0) {
  if (const DslValue *v = find_arg(args, idx, name)) {
    return as_int(*v);
  }
  return def;
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

using MethodHandler =
    std::function<void(ScraperBuilder &, const DslArgList &)>;

const absl::flat_hash_map<std::string_view, MethodHandler> &
get_method_registry() {
  static const absl::flat_hash_map<std::string_view, MethodHandler> registry = {
      {"select",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &sel = require_arg(args, 0, "selector", "select");
         b.Select(as_str(sel), opt_str(args, 1, "key", ""));
       }},
      {"text",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Text(opt_bool(args, 0, "own_text", false));
       }},
      {"attr",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Attr(as_str(require_arg(args, 0, "attr_name", "attr")));
       }},
      {"find_all",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &sel = require_arg(args, 0, "selector", "find_all");
         const auto &type = require_arg(args, 1, "type", "find_all");
         b.FindAll(as_str(sel), parse_value_type(as_str(type)),
                   opt_str(args, 2, "attr", ""));
       }},
      {"find",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &sel = require_arg(args, 0, "selector", "find");
         const auto &type = require_arg(args, 1, "type", "find");
         const auto *num = find_arg(args, 3, "number");
         b.Find(as_str(sel), parse_value_type(as_str(type)),
                opt_str(args, 2, "attr", ""),
                num ? parse_number_type(as_str(*num))
                    : QueryStep::Number::All,
                static_cast<size_t>(opt_int(args, 4, "nth_val", 0)));
       }},
      {"trim", [](ScraperBuilder &b, const DslArgList &) { b.Trim(); }},
      {"strip", [](ScraperBuilder &b, const DslArgList &) { b.Trim(); }},
      {"squish", [](ScraperBuilder &b, const DslArgList &) { b.Squish(); }},
      {"clean", [](ScraperBuilder &b, const DslArgList &) { b.Squish(); }},
      {"normalize_whitespace",
       [](ScraperBuilder &b, const DslArgList &) { b.Squish(); }},
      {"lower", [](ScraperBuilder &b, const DslArgList &) { b.Lower(); }},
      {"upper", [](ScraperBuilder &b, const DslArgList &) { b.Upper(); }},
      {"unescape",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Unescape(opt_bool(args, 0, "url_decode", true));
       }},
      {"replace",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Replace(as_str(require_arg(args, 0, "from", "replace")),
                   opt_str(args, 1, "to", ""));
       }},
      {"replace_exact",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ReplaceExact(as_str(require_arg(args, 0, "from", "replace_exact")),
                        opt_str(args, 1, "to", ""));
       }},
      {"split",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Split(as_str(require_arg(args, 0, "delimiter", "split")),
                 opt_bool(args, 1, "keep_empty_parts", false));
       }},
      {"join",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Join(as_str(require_arg(args, 0, "delimiter", "join")));
       }},
      {"prepend",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Prepend(as_str(require_arg(args, 0, "prefix", "prepend")));
       }},
      {"append",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Append(as_str(require_arg(args, 0, "suffix", "append")));
       }},
      {"drop_empty",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.DropEmpty(opt_bool(args, 0, "also_trim", true));
       }},
      {"substring",
       [](ScraperBuilder &b, const DslArgList &args) {
         const int start = as_int(require_arg(args, 0, "start_pos", "substring"));
         std::optional<int> len;
         if (const DslValue *lv = find_arg(args, 1, "length");
             lv && !lv->is_null()) {
           len = as_int(*lv);
         }
         b.Substring(start, len);
       }},
      {"strip_comments",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.StripComments(opt_bool(args, 0, "multiline", true),
                         opt_bool(args, 1, "single_line", true));
       }},
      {"regex_replace",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "regex_replace");
         const auto &rep = require_arg(args, 1, "replacement", "regex_replace");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexReplace(as_str(pat), as_str(rep), flags);
       }},
      {"sub",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "sub");
         const auto &rep = require_arg(args, 1, "replacement", "sub");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexReplace(as_str(pat), as_str(rep), flags);
       }},
      {"regex_filter",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "regex_filter");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexFilter(as_str(pat), opt_bool(args, 1, "negate", false), flags);
       }},
      {"grep",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "grep");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexFilter(as_str(pat), opt_bool(args, 1, "negate", false), flags);
       }},
      {"regex_extract",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "regex_extract");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexExtract(as_str(pat), opt_int(args, 1, "group_index", 0), flags);
       }},
      {"extract",
       [](ScraperBuilder &b, const DslArgList &args) {
         const auto &pat = require_arg(args, 0, "pattern", "extract");
         std::optional<std::string> flags;
         if (const DslValue *fv = find_arg(args, 2, "flags");
             fv && !fv->is_null()) {
           flags = as_str(*fv);
         }
         b.RegexExtract(as_str(pat), opt_int(args, 1, "group_index", 0), flags);
       }},
      {"filter",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Filter(as_str_list(require_arg(args, 0, "patterns", "filter")));
       }},
      {"set",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Set(as_str(require_arg(args, 0, "name", "set")));
       }},
      {"as",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Set(as_str(require_arg(args, 0, "name", "as")));
       }},
      {"count",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Count(as_str(require_arg(args, 0, "dict_key", "count")));
       }},
      {"first",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.First(static_cast<size_t>(opt_int(args, 0, "count", 1)));
       }},
      {"limit",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.First(static_cast<size_t>(opt_int(args, 0, "count", 1)));
       }},
      {"last",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Last(static_cast<size_t>(opt_int(args, 0, "count", 1)));
       }},
      {"nth",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Nth(static_cast<size_t>(
             as_int(require_arg(args, 0, "index", "nth"))));
       }},
      {"slice",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Slice(
             static_cast<size_t>(
                 as_int(require_arg(args, 0, "start_index", "slice"))),
             static_cast<size_t>(as_int(
                 require_arg(args, 1, "end_index_inclusive", "slice"))));
       }},
      {"unique",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Unique(opt_bool(args, 0, "preserve_order", true));
       }},
      {"dedup",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Unique(opt_bool(args, 0, "preserve_order", true));
       }},
      {"default",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Default(
             as_str(require_arg(args, 0, "default_value", "default")));
       }},
      {"js_block",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsBlock(opt_str(args, 0, "prefix", ""));
       }},
      {"extract_js_block",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsBlock(opt_str(args, 0, "prefix", ""));
       }},
      {"js_blocks",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsBlocks(
             as_str_list(require_arg(args, 0, "prefixes", "js_blocks")));
       }},
      {"extract_js_blocks",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsBlocks(as_str_list(
             require_arg(args, 0, "prefixes", "extract_js_blocks")));
       }},
      {"parse_js",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ParseJs(as_str(require_arg(args, 0, "objname", "parse_js")),
                   opt_bool(args, 1, "simplify", true));
       }},
      {"flatten",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Flatten(opt_bool(args, 0, "postponable", true));
       }},
      {"flatten_local",
       [](ScraperBuilder &b, const DslArgList &) { b.FlattenLocal(); }},
      {"pick_keys",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.PickKeys(
             as_str_list(require_arg(args, 0, "keys_to_keep", "pick_keys")),
             opt_bool(args, 1, "postponable", true));
       }},
      {"omit_keys",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.OmitKeys(
             as_str_list(require_arg(args, 0, "keys_to_remove", "omit_keys")),
             opt_bool(args, 1, "postponable", true));
       }},
      {"rename_keys",
       [](ScraperBuilder &b, const DslArgList &args) {
         const DslValue &mapping = require_arg(args, 0, "pairs", "rename_keys");
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
       [](ScraperBuilder &b, const DslArgList &args) {
         b.MergeKeys(
             as_str_list(require_arg(args, 0, "source_keys", "merge_keys")),
             as_str(require_arg(args, 1, "target_key", "merge_keys")),
             opt_bool(args, 2, "postponable", true));
       }},
      {"sort_keys",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.SortKeys(opt_bool(args, 0, "ascending", true),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"deduplicate",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Deduplicate(opt_bool(args, 0, "postponable", true));
       }},
      {"jmespath",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JmesPath(as_str(require_arg(args, 0, "expr", "jmespath")),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"jsonpath",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPath(as_str(require_arg(args, 0, "expr", "jsonpath")),
                    opt_bool(args, 1, "postponable", true));
       }},
      {"json_parse",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonParse(opt_str(args, 0, "objname", ""),
                     parse_json_opts(args, 1),
                     opt_bool(args, 6, "strict", true));
       }},
      {"json",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonParse(opt_str(args, 0, "objname", ""),
                     parse_json_opts(args, 1),
                     opt_bool(args, 6, "strict", true));
       }},
      {"json_dump",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonDump(opt_bool(args, 0, "pretty", false),
                    opt_int(args, 1, "indent", 2),
                    opt_str(args, 2, "source_key", ""));
       }},
      {"csv_parse",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.CsvParse(opt_str(args, 0, "objname", ""),
                    parse_csv_opts(args, 1),
                    opt_bool(args, 4, "strict", true));
       }},
      {"csv_dump",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.CsvDump(parse_csv_opts(args, 0),
                   opt_str(args, 3, "source_key", ""));
       }},
      {"from_cbor",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.FromCbor(opt_str(args, 0, "objname", ""),
                    parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
                    opt_bool(args, 2, "strict", true));
       }},
      {"to_cbor",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ToCbor(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                  opt_str(args, 1, "source_key", ""));
       }},
      {"from_msgpack",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.FromMsgPack(
             opt_str(args, 0, "objname", ""),
             parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
             opt_bool(args, 2, "strict", true));
       }},
      {"to_msgpack",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ToMsgPack(
             parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
             opt_str(args, 1, "source_key", ""));
       }},
      {"from_bson",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.FromBson(opt_str(args, 0, "objname", ""),
                    parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
                    opt_bool(args, 2, "strict", true));
       }},
      {"to_bson",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ToBson(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                  opt_str(args, 1, "source_key", ""));
       }},
      {"from_ubjson",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.FromUbjson(
             opt_str(args, 0, "objname", ""),
             parse_binary_encoding(opt_str(args, 1, "encoding", "hex")),
             opt_bool(args, 2, "strict", true));
       }},
      {"to_ubjson",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.ToUbjson(parse_binary_encoding(opt_str(args, 0, "encoding", "hex")),
                    opt_str(args, 1, "source_key", ""));
       }},
      {"jsonpath_replace",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPathReplace(
             as_str(require_arg(args, 0, "expr", "jsonpath_replace")),
             dsl_value_to_json(
                 require_arg(args, 1, "replacement", "jsonpath_replace")),
             opt_bool(args, 2, "postponable", true));
       }},
      {"json_flatten",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonFlatten(opt_bool(args, 0, "postponable", true));
       }},
      {"json_unflatten",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonUnflatten(opt_bool(args, 0, "postponable", true));
       }},
      {"jsonpointer_get",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPointerGet(
             as_str(require_arg(args, 0, "pointer", "jsonpointer_get")),
             opt_str(args, 1, "target_key", ""),
             opt_bool(args, 2, "postponable", true));
       }},
      {"jsonpointer_set",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPointerSet(
             as_str(require_arg(args, 0, "pointer", "jsonpointer_set")),
             dsl_value_to_json(
                 require_arg(args, 1, "value", "jsonpointer_set")),
             opt_bool(args, 2, "postponable", true));
       }},
      {"jsonpointer_remove",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPointerRemove(
             as_str(require_arg(args, 0, "pointer", "jsonpointer_remove")),
             opt_bool(args, 1, "postponable", true));
       }},
      {"json_patch",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonPatch(
             as_json_document(require_arg(args, 0, "patch", "json_patch")),
             opt_bool(args, 1, "postponable", true));
       }},
      {"merge_patch",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.MergePatch(
             as_json_document(require_arg(args, 0, "patch", "merge_patch")),
             opt_bool(args, 1, "postponable", true));
       }},
      {"json_schema",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.JsonSchema(
             as_json_document(require_arg(args, 0, "schema", "json_schema")),
             opt_bool(args, 1, "strict", true),
             opt_str(args, 2, "error_key", "_schema_errors"),
             opt_bool(args, 3, "postponable", true));
       }},
      {"debug",
       [](ScraperBuilder &b, const DslArgList &args) {
         b.Debug(opt_str(args, 0, "prefix", "DEBUG: "),
                 opt_bool(args, 1, "print_dict", false),
                 opt_bool(args, 2, "print_node", false));
       }},
  };
  return registry;
}

} // namespace

DslValue::DslValue(std::string name, bool is_ident) {
  if (is_ident) {
    value = Identifier{std::move(name)};
  } else {
    value = std::move(name);
  }
}

std::string DslValue::to_string() const {
  if (is_null())
    return "null";
  if (is_string())
    return get_string();
  if (is_int())
    return std::to_string(get_int());
  if (is_double()) {
    std::ostringstream oss;
    oss << get_double();
    return oss.str();
  }
  if (is_bool())
    return get_bool() ? "true" : "false";
  if (is_identifier())
    return get_identifier();
  if (is_object())
    return "<object>";
  return "<list>";
}

char Lexer::peek() const noexcept {
  return pos_ < src_.size() ? src_[pos_] : '\0';
}

char Lexer::advance() noexcept {
  const char c = peek();
  if (c == '\n') {
    ++line_;
    col_ = 1;
  } else {
    ++col_;
  }
  if (pos_ < src_.size()) {
    ++pos_;
  }
  return c;
}

void Lexer::skip_whitespace_and_comments() noexcept {
  while (pos_ < src_.size()) {
    const char c = peek();
    if (std::isspace(static_cast<unsigned char>(c))) {
      advance();
    } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
      while (pos_ < src_.size() && peek() != '\n')
        advance();
    } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '*') {
      advance();
      advance();
      while (pos_ < src_.size() &&
             !(peek() == '*' && pos_ + 1 < src_.size() &&
               src_[pos_ + 1] == '/')) {
        advance();
      }
      if (pos_ < src_.size()) {
        advance();
        advance();
      }
    } else if (c == '#') {
      while (pos_ < src_.size() && peek() != '\n')
        advance();
    } else {
      break;
    }
  }
}

Token Lexer::next_token() {
  skip_whitespace_and_comments();
  const int tok_line = line_;
  const int tok_col = col_;
  if (pos_ >= src_.size()) {
    return {Token::Type::END_OF_FILE, "", tok_line, tok_col};
  }

  const char c = peek();
  if (c == ';') {
    advance();
    return {Token::Type::SEMICOLON, ";", tok_line, tok_col};
  }
  if (c == ',') {
    advance();
    return {Token::Type::COMMA, ",", tok_line, tok_col};
  }
  if (c == '.') {
    advance();
    return {Token::Type::DOT, ".", tok_line, tok_col};
  }
  if (c == '(') {
    advance();
    return {Token::Type::LPAREN, "(", tok_line, tok_col};
  }
  if (c == ')') {
    advance();
    return {Token::Type::RPAREN, ")", tok_line, tok_col};
  }
  if (c == '{') {
    advance();
    return {Token::Type::LBRACE, "{", tok_line, tok_col};
  }
  if (c == '}') {
    advance();
    return {Token::Type::RBRACE, "}", tok_line, tok_col};
  }
  if (c == '[') {
    advance();
    return {Token::Type::LBRACKET, "[", tok_line, tok_col};
  }
  if (c == ']') {
    advance();
    return {Token::Type::RBRACKET, "]", tok_line, tok_col};
  }
  if (c == '=') {
    advance();
    return {Token::Type::ASSIGN, "=", tok_line, tok_col};
  }
  if (c == ':') {
    advance();
    if (peek() == '=') {
      advance();
      return {Token::Type::COLON_ASSIGN, ":=", tok_line, tok_col};
    }
    return {Token::Type::COLON, ":", tok_line, tok_col};
  }
  if (c == '|') {
    advance();
    if (peek() == '>') {
      advance();
      return {Token::Type::PIPE, "|>", tok_line, tok_col};
    }
    return {Token::Type::PIPE, "|", tok_line, tok_col};
  }
  if (c == '$') {
    advance();
    return {Token::Type::DOLLAR, "$", tok_line, tok_col};
  }
  if (c == '@') {
    advance();
    return {Token::Type::AT, "@", tok_line, tok_col};
  }
  if (c == '?' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '?') {
    advance();
    advance();
    return {Token::Type::DOUBLE_QUESTION, "??", tok_line, tok_col};
  }
  if (c == '-' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '>') {
    advance();
    advance();
    return {Token::Type::ARROW, "->", tok_line, tok_col};
  }
  if (c == '>' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '>') {
    advance();
    advance();
    return {Token::Type::DOUBLE_ARROW, ">>", tok_line, tok_col};
  }

  if (c == '"' || c == '\'') {
    const char quote = advance();
    std::string val;
    while (pos_ < src_.size() && peek() != quote) {
      if (peek() == '\\') {
        advance();
        if (pos_ < src_.size()) {
          const char esc = advance();
          switch (esc) {
          case 'n':
            val += '\n';
            break;
          case 't':
            val += '\t';
            break;
          case 'r':
            val += '\r';
            break;
          case '$':
            // Preserve escaped \$ so interpolate_string() keeps literal '$'
            val.push_back('\0');
            val.push_back('$');
            break;
          default:
            val += esc;
            break;
          }
        }
      } else {
        val += advance();
      }
    }
    if (peek() != quote) {
      throw std::runtime_error("Unterminated string literal in DSL at line " +
                               std::to_string(tok_line));
    }
    advance();
    return {Token::Type::STRING, std::move(val), tok_line, tok_col};
  }

  if (std::isdigit(static_cast<unsigned char>(c)) ||
      (c == '-' && pos_ + 1 < src_.size() &&
       std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
    std::string num;
    if (c == '-')
      num += advance();
    while (pos_ < src_.size() &&
           std::isdigit(static_cast<unsigned char>(peek()))) {
      num += advance();
    }
    if (peek() == '.' && pos_ + 1 < src_.size() &&
        std::isdigit(static_cast<unsigned char>(src_[pos_ + 1]))) {
      num += advance();
      while (pos_ < src_.size() &&
             std::isdigit(static_cast<unsigned char>(peek()))) {
        num += advance();
      }
      return {Token::Type::FLOAT, std::move(num), tok_line, tok_col};
    }
    return {Token::Type::NUMBER, std::move(num), tok_line, tok_col};
  }

  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
    std::string ident;
    while (pos_ < src_.size()) {
      const char ch = peek();
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' ||
          ch == '.') {
        if (ch == '.') {
          break;
        }
        ident += advance();
      } else {
        break;
      }
    }
    if (peek() == '.' && (ident == "Value" || ident == "Number")) {
      ident += advance();
      while (pos_ < src_.size() &&
             (std::isalnum(static_cast<unsigned char>(peek())) ||
              peek() == '_')) {
        ident += advance();
      }
    }
    if (ident == "let")
      return {Token::Type::LET, ident, tok_line, tok_col};
    if (ident == "true" || ident == "false")
      return {Token::Type::BOOL, ident, tok_line, tok_col};
    if (ident == "null")
      return {Token::Type::NULL_LIT, ident, tok_line, tok_col};
    return {Token::Type::IDENTIFIER, std::move(ident), tok_line, tok_col};
  }

  throw std::runtime_error("Unexpected character in DSL at line " +
                           std::to_string(tok_line) + ": '" + std::string(1, c) +
                           "'");
}

void DslParser::advance_token() { current_ = lexer_->next_token(); }

void DslParser::expect(Token::Type type, std::string_view msg) {
  if (current_.type != type) {
    throw std::runtime_error(std::string(msg) + " at line " +
                             std::to_string(current_.line) + ", got '" +
                             current_.value + "'");
  }
  advance_token();
}

std::string DslParser::interpolate_string(const std::string &raw) const {
  std::string out;
  out.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\0' && i + 1 < raw.size() && raw[i + 1] == '$') {
      out.push_back('$');
      ++i;
      continue;
    }
    if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{') {
      const size_t close = raw.find('}', i + 2);
      if (close == std::string::npos) {
        throw std::runtime_error("Unterminated '${...}' interpolation in DSL");
      }
      const std::string var_name = raw.substr(i + 2, close - (i + 2));
      const auto it = variables_.find(var_name);
      if (it == variables_.end()) {
        throw std::runtime_error("Undefined variable in string interpolation: " +
                                 var_name);
      }
      out += it->second.to_string();
      i = close;
      continue;
    }
    out.push_back(raw[i]);
  }
  return out;
}

DslValue DslParser::parse_arg(int depth) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL nesting depth exceeded");
  }

  if (current_.type == Token::Type::STRING) {
    DslValue v(interpolate_string(current_.value));
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::NUMBER) {
    DslValue v(std::stoi(current_.value));
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::FLOAT) {
    DslValue v(std::stod(current_.value));
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::BOOL) {
    DslValue v(current_.value == "true");
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::NULL_LIT) {
    advance_token();
    return DslValue(std::monostate{});
  }
  if (current_.type == Token::Type::IDENTIFIER) {
    const std::string name = current_.value;
    advance_token();
    auto it = variables_.find(name);
    if (it != variables_.end()) {
      return it->second;
    }
    return DslValue(name, true);
  }
  if (current_.type == Token::Type::LBRACKET) {
    advance_token();
    std::vector<DslValue> items;
    while (current_.type != Token::Type::RBRACKET &&
           current_.type != Token::Type::END_OF_FILE) {
      items.push_back(parse_arg(depth + 1));
      if (current_.type == Token::Type::COMMA) {
        advance_token();
      } else {
        break;
      }
    }
    expect(Token::Type::RBRACKET, "Expected ']'");
    return DslValue(std::move(items));
  }
  if (current_.type == Token::Type::LBRACE) {
    advance_token();
    DslValue::ObjectType obj;
    while (current_.type != Token::Type::RBRACE &&
           current_.type != Token::Type::END_OF_FILE) {
      std::string key;
      if (current_.type == Token::Type::STRING) {
        key = interpolate_string(current_.value);
        advance_token();
      } else if (current_.type == Token::Type::IDENTIFIER) {
        key = current_.value;
        advance_token();
      } else {
        throw std::runtime_error(
            "Expected string or identifier key in object literal at line " +
            std::to_string(current_.line));
      }
      expect(Token::Type::COLON, "Expected ':' after object key");
      DslValue val = parse_arg(depth + 1);
      obj.emplace_back(std::move(key), std::move(val));
      if (current_.type == Token::Type::COMMA) {
        advance_token();
      } else {
        break;
      }
    }
    expect(Token::Type::RBRACE, "Expected '}'");
    return DslValue(std::move(obj));
  }
  throw std::runtime_error("Unexpected token in DSL argument at line " +
                           std::to_string(current_.line) + ": '" +
                           current_.value + "'");
}

DslArgList DslParser::parse_arg_list(int depth) {
  DslArgList args;
  expect(Token::Type::LPAREN, "Expected '('");
  bool seen_named = false;
  while (current_.type != Token::Type::RPAREN &&
         current_.type != Token::Type::END_OF_FILE) {
    if (current_.type == Token::Type::IDENTIFIER) {
      const std::string ident_name = current_.value;
      const int ident_line = current_.line;
      advance_token();
      if (current_.type == Token::Type::ASSIGN) {
        advance_token();
        seen_named = true;
        DslValue val = parse_arg(depth + 1);
        if (!args.named.emplace(ident_name, std::move(val)).second) {
          throw std::runtime_error("Duplicate keyword argument '" + ident_name +
                                   "' at line " + std::to_string(ident_line));
        }
      } else {
        if (seen_named) {
          throw std::runtime_error(
              "Positional argument cannot follow keyword argument at line " +
              std::to_string(ident_line));
        }
        auto it = variables_.find(ident_name);
        if (it != variables_.end()) {
          args.positional.push_back(it->second);
        } else {
          args.positional.emplace_back(ident_name, true);
        }
      }
    } else {
      if (seen_named) {
        throw std::runtime_error(
            "Positional argument cannot follow keyword argument at line " +
            std::to_string(current_.line));
      }
      args.positional.push_back(parse_arg(depth + 1));
    }

    if (current_.type == Token::Type::COMMA) {
      advance_token();
    } else {
      break;
    }
  }
  expect(Token::Type::RPAREN, "Expected ')'");
  return args;
}

void DslParser::apply_method(ScraperBuilder &builder, std::string_view name,
                             const DslArgList &args) {
  const auto &reg = get_method_registry();
  const auto it = reg.find(name);
  if (it == reg.end()) {
    throw std::runtime_error("Unknown DSL method: " + std::string(name));
  }
  it->second(builder, args);
}

void DslParser::parse_chain(ScraperBuilder &builder, int depth, bool in_block) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL chain depth exceeded");
  }
  bool first_in_block = in_block;
  while (true) {
    if (current_.type == Token::Type::ARROW ||
        current_.type == Token::Type::DOUBLE_ARROW) {
      advance_token();
      DslValue key = parse_arg(depth + 1);
      builder.Set(as_str(key));
      first_in_block = false;
      continue;
    }
    if (current_.type == Token::Type::DOUBLE_QUESTION) {
      advance_token();
      DslValue def_val = parse_arg(depth + 1);
      builder.Default(as_str(def_val));
      first_in_block = false;
      continue;
    }
    if (current_.type == Token::Type::IDENTIFIER && current_.value == "as") {
      advance_token();
      if (current_.type == Token::Type::LPAREN) {
        apply_method(builder, "as", parse_arg_list(depth + 1));
      } else {
        DslValue key = parse_arg(depth + 1);
        builder.Set(as_str(key));
      }
      first_in_block = false;
      continue;
    }

    bool has_sep = false;
    if (current_.type == Token::Type::DOT ||
        current_.type == Token::Type::PIPE) {
      has_sep = true;
      advance_token();
    } else if (!first_in_block) {
      break;
    } else if (current_.type == Token::Type::RBRACE ||
               current_.type == Token::Type::END_OF_FILE) {
      break;
    }
    first_in_block = false;

    if (has_sep && current_.type == Token::Type::LPAREN) {
      advance_token();
      parse_chain(builder, depth + 1, false);
      expect(Token::Type::RPAREN, "Expected ')' after group");
      continue;
    }
    if (current_.type == Token::Type::DOLLAR) {
      advance_token();
      if (current_.type == Token::Type::LPAREN) {
        apply_method(builder, "select", parse_arg_list(depth + 1));
      } else {
        DslValue sel = parse_arg(depth + 1);
        builder.Select(as_str(sel));
      }
      if (current_.type == Token::Type::LBRACE) {
        advance_token();
        parse_chain(builder, depth + 1, true);
        expect(Token::Type::RBRACE, "Expected '}' after select block");
      }
      continue;
    }
    if (current_.type == Token::Type::AT) {
      advance_token();
      if (current_.type == Token::Type::LPAREN) {
        apply_method(builder, "attr", parse_arg_list(depth + 1));
      } else {
        DslValue attr_name = parse_arg(depth + 1);
        builder.Attr(as_str(attr_name));
      }
      continue;
    }
    if (current_.type == Token::Type::LBRACKET) {
      advance_token();
      const int idx = as_int(parse_arg(depth + 1));
      expect(Token::Type::RBRACKET, "Expected ']' after index");
      if (idx >= 0) {
        builder.Nth(static_cast<size_t>(idx));
      } else if (idx == -1) {
        builder.Last(1);
      } else {
        throw std::runtime_error(
            "Negative index other than -1 is not supported in DSL chain");
      }
      continue;
    }
    if (current_.type == Token::Type::DOUBLE_QUESTION) {
      advance_token();
      DslValue def_val = parse_arg(depth + 1);
      builder.Default(as_str(def_val));
      continue;
    }
    if (current_.type != Token::Type::IDENTIFIER) {
      throw std::runtime_error("Expected method name in chain at line " +
                               std::to_string(current_.line));
    }
    const std::string method = current_.value;
    advance_token();
    DslArgList args;
    if (current_.type == Token::Type::LPAREN) {
      args = parse_arg_list(depth + 1);
    }
    apply_method(builder, method, args);

    if (method == "select" && current_.type == Token::Type::LBRACE) {
      advance_token();
      parse_chain(builder, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after select block");
    }
  }
}

absl::flat_hash_map<std::string, ScraperBuilder>
DslParser::parse(const std::string &dsl_source) {
  variables_.clear();
  scrapers_.clear();
  Lexer lex(dsl_source);
  lexer_ = &lex;
  advance_token();

  while (current_.type != Token::Type::END_OF_FILE) {
    if (current_.type == Token::Type::LET) {
      advance_token();
      if (current_.type != Token::Type::IDENTIFIER) {
        throw std::runtime_error("Expected variable name after 'let'");
      }
      const std::string var_name = current_.value;
      advance_token();
      expect(Token::Type::ASSIGN, "Expected '=' after variable name");
      variables_[var_name] = parse_arg();
      if (current_.type == Token::Type::SEMICOLON)
        advance_token();
      continue;
    }

    if (current_.type == Token::Type::IDENTIFIER) {
      const std::string name = current_.value;
      advance_token();
      if (current_.type == Token::Type::ASSIGN ||
          current_.type == Token::Type::COLON ||
          current_.type == Token::Type::COLON_ASSIGN) {
        advance_token();
      }
      if (current_.type == Token::Type::IDENTIFIER &&
          (current_.value == "Scraper" || current_.value == "ScraperPy")) {
        advance_token();
        if (current_.type == Token::Type::LPAREN) {
          (void)parse_arg_list();
        }
      }
      ScraperBuilder builder;
      parse_chain(builder);
      scrapers_.insert_or_assign(name, std::move(builder));
      if (current_.type == Token::Type::SEMICOLON)
        advance_token();
      continue;
    }

    throw std::runtime_error("Unexpected token at top level in DSL at line " +
                             std::to_string(current_.line) + ": '" +
                             current_.value + "'");
  }

  return std::move(scrapers_);
}

absl::flat_hash_map<std::string, ScraperBuilder>
DslParser::parse_file(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open DSL file: " + filepath);
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  return parse(ss.str());
}

} // namespace scraper::dsl
