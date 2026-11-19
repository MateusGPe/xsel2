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

std::string as_str(const DslValue &v) {
  if (v.is_string())
    return v.get_string();
  if (v.is_identifier())
    return v.get_identifier();
  if (v.is_int())
    return std::to_string(v.get_int());
  if (v.is_bool())
    return v.get_bool() ? "true" : "false";
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

using MethodHandler =
    std::function<void(ScraperBuilder &, const std::vector<DslValue> &)>;

const absl::flat_hash_map<std::string_view, MethodHandler> &
get_method_registry() {
  static const absl::flat_hash_map<std::string_view, MethodHandler> registry = {
      {"push",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("push() requires at least 1 argument");
         b.Group(as_str(args[0]), args.size() > 1 ? as_str(args[1]) : "");
       }},
      {"text",
       [](ScraperBuilder &b, const auto &args) {
         b.Text(args.empty() ? false : as_bool(args[0]));
       }},
      {"attr",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("attr() requires 1 argument");
         b.Attr(as_str(args[0]));
       }},
      {"query_all",
       [](ScraperBuilder &b, const auto &args) {
         if (args.size() < 2)
           throw std::runtime_error("query_all() requires 2+ arguments");
         b.QueryAll(as_str(args[0]), parse_value_type(as_str(args[1])),
                    args.size() > 2 ? as_str(args[2]) : "");
       }},
      {"query",
       [](ScraperBuilder &b, const auto &args) {
         if (args.size() < 2)
           throw std::runtime_error("query() requires 2+ arguments");
         b.Query(as_str(args[0]), parse_value_type(as_str(args[1])),
                 args.size() > 2 ? as_str(args[2]) : "",
                 args.size() > 3 ? parse_number_type(as_str(args[3]))
                                 : QueryStep::Number::All,
                 args.size() > 4 ? static_cast<size_t>(as_int(args[4])) : 0);
       }},
      {"trim", [](ScraperBuilder &b, const auto &) { b.Trim(); }},
      {"strip", [](ScraperBuilder &b, const auto &) { b.Normspace(); }},
      {"normspace", [](ScraperBuilder &b, const auto &) { b.Normspace(); }},
      {"lower", [](ScraperBuilder &b, const auto &) { b.Lower(); }},
      {"upper", [](ScraperBuilder &b, const auto &) { b.Upper(); }},
      {"unescape",
       [](ScraperBuilder &b, const auto &args) {
         b.Unescape(args.empty() ? true : as_bool(args[0]));
       }},
      {"replace",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("replace() requires 1+ arguments");
         b.Replace(as_str(args[0]), args.size() > 1 ? as_str(args[1]) : "");
       }},
      {"substitute",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("substitute() requires 1+ arguments");
         b.Substitute(as_str(args[0]), args.size() > 1 ? as_str(args[1]) : "");
       }},
      {"split",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("split() requires 1+ arguments");
         b.Split(as_str(args[0]), args.size() > 1 ? as_bool(args[1]) : false);
       }},
      {"join",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("join() requires 1 argument");
         b.Join(as_str(args[0]));
       }},
      {"prepend",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("prepend() requires 1 argument");
         b.Prepend(as_str(args[0]));
       }},
      {"append",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("append() requires 1 argument");
         b.Append(as_str(args[0]));
       }},
      {"remove_empty",
       [](ScraperBuilder &b, const auto &args) {
         b.RemoveEmpty(args.empty() ? true : as_bool(args[0]));
       }},
      {"substring",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("substring() requires 1+ arguments");
         std::optional<int> len;
         if (args.size() > 1)
           len = as_int(args[1]);
         b.Substring(as_int(args[0]), len);
       }},
      {"strip_comments",
       [](ScraperBuilder &b, const auto &args) {
         b.StripComments(args.empty() ? true : as_bool(args[0]),
                         args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"regex_replace",
       [](ScraperBuilder &b, const auto &args) {
         if (args.size() < 2)
           throw std::runtime_error("regex_replace() requires 2+ arguments");
         std::optional<std::string> flags;
         if (args.size() > 2)
           flags = as_str(args[2]);
         b.RegexReplace(as_str(args[0]), as_str(args[1]), flags);
       }},
      {"regex_filter",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("regex_filter() requires 1+ arguments");
         std::optional<std::string> flags;
         if (args.size() > 2)
           flags = as_str(args[2]);
         b.RegexFilter(as_str(args[0]),
                       args.size() > 1 ? as_bool(args[1]) : false, flags);
       }},
      {"regex_extract",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("regex_extract() requires 1+ arguments");
         std::optional<std::string> flags;
         if (args.size() > 2)
           flags = as_str(args[2]);
         b.RegexExtract(as_str(args[0]),
                        args.size() > 1 ? as_int(args[1]) : 0, flags);
       }},
      {"filter",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("filter() requires 1 argument");
         b.Filter(as_str_list(args[0]));
       }},
      {"set",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("set() requires 1 argument");
         b.Set(as_str(args[0]));
       }},
      {"count_values",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("count_values() requires 1 argument");
         b.CountValues(as_str(args[0]));
       }},
      {"first",
       [](ScraperBuilder &b, const auto &args) {
         b.First(args.empty() ? 1 : static_cast<size_t>(as_int(args[0])));
       }},
      {"last",
       [](ScraperBuilder &b, const auto &args) {
         b.Last(args.empty() ? 1 : static_cast<size_t>(as_int(args[0])));
       }},
      {"nth",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("nth() requires 1 argument");
         b.Nth(static_cast<size_t>(as_int(args[0])));
       }},
      {"slice_values",
       [](ScraperBuilder &b, const auto &args) {
         if (args.size() < 2)
           throw std::runtime_error("slice_values() requires 2 arguments");
         b.SliceValues(static_cast<size_t>(as_int(args[0])),
                       static_cast<size_t>(as_int(args[1])));
       }},
      {"unique",
       [](ScraperBuilder &b, const auto &args) {
         b.Unique(args.empty() ? true : as_bool(args[0]));
       }},
      {"default_if_empty",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("default_if_empty() requires 1 argument");
         b.DefaultIfEmpty(as_str(args[0]));
       }},
      {"block_seeker",
       [](ScraperBuilder &b, const auto &args) {
         b.BlockSeeker(args.empty() ? "" : as_str(args[0]));
       }},
      {"block_parser",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("block_parser() requires 1 argument");
         b.BlockParser(as_str_list(args[0]));
       }},
      {"parse",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("parse() requires 1+ arguments");
         b.Parse(as_str(args[0]), args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"flatten_dict",
       [](ScraperBuilder &b, const auto &args) {
         b.FlattenDict(args.empty() ? true : as_bool(args[0]));
       }},
      {"flatten_local", [](ScraperBuilder &b, const auto &) { b.FlattenLocal(); }},
      {"filter_dict",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("filter_dict() requires 1+ arguments");
         b.FilterDict(as_str_list(args[0]),
                      args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"remove_keys",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("remove_keys() requires 1+ arguments");
         b.RemoveKeys(as_str_list(args[0]),
                      args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"rename_keys",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("rename_keys() requires 1+ arguments");
         const auto pairs = as_str_list(args[0]);
         if (pairs.size() % 2 != 0) {
           throw std::runtime_error(
               "rename_keys() requires an even-length list of [old, new] pairs");
         }
         absl::flat_hash_map<std::string, std::string> rename_map;
         rename_map.reserve(pairs.size() / 2);
         for (size_t i = 0; i < pairs.size(); i += 2) {
           rename_map.emplace(pairs[i], pairs[i + 1]);
         }
         b.RenameKeys(std::move(rename_map),
                      args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"merge_keys",
       [](ScraperBuilder &b, const auto &args) {
         if (args.size() < 2)
           throw std::runtime_error("merge_keys() requires 2+ arguments");
         b.MergeKeys(as_str_list(args[0]), as_str(args[1]),
                     args.size() > 2 ? as_bool(args[2]) : true);
       }},
      {"sort_keys",
       [](ScraperBuilder &b, const auto &args) {
         b.SortKeys(args.empty() ? true : as_bool(args[0]),
                    args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"deduplicate",
       [](ScraperBuilder &b, const auto &args) {
         b.Deduplicate(args.empty() ? true : as_bool(args[0]));
       }},
      {"jmespath",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("jmespath() requires 1+ arguments");
         b.JmesPath(as_str(args[0]),
                    args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"jsonpath",
       [](ScraperBuilder &b, const auto &args) {
         if (args.empty())
           throw std::runtime_error("jsonpath() requires 1+ arguments");
         b.JsonPath(as_str(args[0]),
                    args.size() > 1 ? as_bool(args[1]) : true);
       }},
      {"debug_print",
       [](ScraperBuilder &b, const auto &args) {
         b.DebugPrint(args.empty() ? "DEBUG: " : as_str(args[0]),
                      args.size() > 1 ? as_bool(args[1]) : false,
                      args.size() > 2 ? as_bool(args[2]) : false);
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
  if (is_string())
    return get_string();
  if (is_int())
    return std::to_string(get_int());
  if (is_bool())
    return get_bool() ? "true" : "false";
  if (is_identifier())
    return get_identifier();
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
  if (c == ':' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '=') {
    advance();
    advance();
    return {Token::Type::COLON_ASSIGN, ":=", tok_line, tok_col};
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
    return {Token::Type::NUMBER, std::move(num), tok_line, tok_col};
  }

  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
    std::string ident;
    while (pos_ < src_.size()) {
      const char ch = peek();
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' ||
          ch == '.') {
        // Stop before '.' if it's followed by '(' or whitespace (method chain)
        if (ch == '.') {
          break;
        }
        ident += advance();
      } else {
        break;
      }
    }
    // Support dotted enum names like Value.Text / Number.Nth
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

DslValue DslParser::parse_arg(int depth) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL nesting depth exceeded");
  }

  if (current_.type == Token::Type::STRING) {
    DslValue v(current_.value);
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::NUMBER) {
    DslValue v(std::stoi(current_.value));
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::BOOL) {
    DslValue v(current_.value == "true");
    advance_token();
    return v;
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
  throw std::runtime_error("Unexpected token in DSL argument at line " +
                           std::to_string(current_.line) + ": '" +
                           current_.value + "'");
}

std::vector<DslValue> DslParser::parse_arg_list(int depth) {
  std::vector<DslValue> args;
  expect(Token::Type::LPAREN, "Expected '('");
  while (current_.type != Token::Type::RPAREN &&
         current_.type != Token::Type::END_OF_FILE) {
    args.push_back(parse_arg(depth + 1));
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
                             const std::vector<DslValue> &args) {
  const auto &reg = get_method_registry();
  const auto it = reg.find(name);
  if (it == reg.end()) {
    throw std::runtime_error("Unknown DSL method: " + std::string(name));
  }
  it->second(builder, args);
}

void DslParser::parse_chain(ScraperBuilder &builder, int depth) {
  if (depth > MAX_DSL_DEPTH) {
    throw std::runtime_error("Maximum DSL chain depth exceeded");
  }
  while (current_.type == Token::Type::DOT) {
    advance_token();
    if (current_.type == Token::Type::LPAREN) {
      advance_token();
      parse_chain(builder, depth + 1);
      expect(Token::Type::RPAREN, "Expected ')' after group");
      continue;
    }
    if (current_.type != Token::Type::IDENTIFIER) {
      throw std::runtime_error("Expected method name after '.' at line " +
                               std::to_string(current_.line));
    }
    const std::string method = current_.value;
    advance_token();
    auto args = parse_arg_list(depth + 1);
    apply_method(builder, method, args);
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
