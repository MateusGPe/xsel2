#include "scraper/dsl_parser.hpp"
#include "scraper/dsl/step_registry.hpp"
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace scraper::dsl {

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
    DslValue v(parse_strict_int(current_.value,
                                "DSL line " + std::to_string(current_.line)));
    advance_token();
    return v;
  }
  if (current_.type == Token::Type::FLOAT) {
    double d = 0.0;
    const auto [ptr, ec] = std::from_chars(
        current_.value.data(), current_.value.data() + current_.value.size(), d);
    if (ec != std::errc{} ||
        ptr != current_.value.data() + current_.value.size() ||
        !std::isfinite(d)) {
      throw std::runtime_error("Non-finite or invalid float literal in DSL at line " +
                               std::to_string(current_.line));
    }
    DslValue v(d);
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
  apply_dsl_method(builder, name, args);
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
        if (attr_name.is_list()) {
          builder.AttrFallback(as_str_list(attr_name));
        } else {
          builder.Attr(as_str(attr_name));
        }
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

    if (method == "tee" || method == "branch") {
      validate_args_consumed(args, method);
      expect(Token::Type::LBRACE, "Expected '{' after tee/branch");
      ScraperBuilder sub;
      sub.Select("");
      parse_chain(sub, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after tee/branch block");
      builder.Tee(sub.get_extractors().front()->steps());
      continue;
    }

    if (method == "each") {
      const std::string sel =
          as_str(require_arg(args, 0, "selector", method));
      const std::string key = opt_str(args, 1, "key", "");
      validate_args_consumed(args, method);
      expect(Token::Type::LBRACE, "Expected '{' after each(...)");
      ScraperBuilder sub;
      sub.Select("");
      parse_chain(sub, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after each block");
      builder.Each(sel, key, sub.get_extractors().front()->steps());
      continue;
    }

    if (method == "when_exists" || method == "unless_exists") {
      const std::string sel =
          as_str(require_arg(args, 0, "selector", method));
      validate_args_consumed(args, method);
      expect(Token::Type::LBRACE, "Expected '{' after " + method);
      ScraperBuilder sub;
      sub.Select("");
      parse_chain(sub, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after " + method + " block");
      builder.WhenExists(sel, sub.get_extractors().front()->steps(),
                         method == "unless_exists");
      continue;
    }

    if (method == "when_match") {
      const std::string pat =
          as_str(require_arg(args, 0, "pattern", method));
      const bool negate = opt_bool(args, 1, "negate", false);
      std::optional<std::string> flags;
      if (const DslValue *fv = find_arg(args, 2, "flags");
          fv && !fv->is_null()) {
        flags = as_str(*fv);
      }
      validate_args_consumed(args, method);
      expect(Token::Type::LBRACE, "Expected '{' after when_match");
      ScraperBuilder sub;
      sub.Select("");
      parse_chain(sub, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after when_match block");
      builder.WhenMatch(pat, sub.get_extractors().front()->steps(), negate,
                        flags);
      continue;
    }

    if (method == "finalize") {
      validate_args_consumed(args, method);
      expect(Token::Type::LBRACE, "Expected '{' after finalize");
      ScraperBuilder sub;
      sub.Select("");
      parse_chain(sub, depth + 1, true);
      expect(Token::Type::RBRACE, "Expected '}' after finalize block");
      builder.Finalize(sub.get_extractors().front()->steps());
      continue;
    }

    if (method == "use") {
      const std::string pipe_name =
          as_str(require_arg(args, 0, "name", method));
      validate_args_consumed(args, method);
      const auto it = pipes_.find(pipe_name);
      if (it == pipes_.end()) {
        throw std::runtime_error("Unknown pipe in DSL: '" + pipe_name + "'");
      }
      for (const auto &step : it->second) {
        builder.AddStep(step);
      }
      continue;
    }

    if (method == "include") {
      const std::string scraper_name =
          as_str(require_arg(args, 0, "scraper", method));
      validate_args_consumed(args, method);
      const auto it = scrapers_.find(scraper_name);
      if (it == scrapers_.end()) {
        throw std::runtime_error("Unknown scraper in include(): '" +
                                 scraper_name + "'");
      }
      builder.Include(it->second);
      continue;
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
  pipes_.clear();
  Lexer lex(dsl_source);
  struct LexerGuard {
    Lexer *&slot;
    ~LexerGuard() { slot = nullptr; }
  } guard{lexer_};
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
      if (current_.type == Token::Type::SEMICOLON) {
        advance_token();
      }
      continue;
    }

    if (current_.type == Token::Type::IDENTIFIER) {
      const std::string name = current_.value;
      advance_token();
      if (name == "pipe" && current_.type == Token::Type::IDENTIFIER &&
          current_.value != "Scraper" && current_.value != "ScraperPy") {
        const std::string pipe_name = current_.value;
        advance_token();
        if (current_.type == Token::Type::ASSIGN ||
            current_.type == Token::Type::COLON ||
            current_.type == Token::Type::COLON_ASSIGN) {
          advance_token();
        }
        expect(Token::Type::LBRACE, "Expected '{' after pipe name");
        ScraperBuilder temp;
        temp.Select("");
        parse_chain(temp, 0, true);
        expect(Token::Type::RBRACE, "Expected '}' after pipe block");
        pipes_.insert_or_assign(pipe_name,
                                temp.get_extractors().front()->steps());
        if (current_.type == Token::Type::SEMICOLON) {
          advance_token();
        }
        continue;
      }
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
      if (current_.type == Token::Type::SEMICOLON) {
        advance_token();
      }
      continue;
    }

    throw std::runtime_error("Unexpected token at top level in DSL at line " +
                             std::to_string(current_.line) + ": '" +
                             current_.value + "'");
  }

  auto result = std::move(scrapers_);
  scrapers_.clear();
  return result;
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
