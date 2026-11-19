#pragma once

#include "scraper/scraper.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scraper::dsl {

inline constexpr int MAX_DSL_DEPTH = 256;

struct DslValue {
  struct Identifier {
    std::string name;
  };

  using ValueType =
      std::variant<std::string, int, bool, std::vector<DslValue>, Identifier>;

  ValueType value;

  DslValue() : value(std::string("")) {}
  explicit DslValue(std::string s) : value(std::move(s)) {}
  explicit DslValue(int i) : value(i) {}
  explicit DslValue(bool b) : value(b) {}
  explicit DslValue(std::vector<DslValue> l) : value(std::move(l)) {}
  DslValue(std::string name, bool is_ident);

  [[nodiscard]] bool is_string() const {
    return std::holds_alternative<std::string>(value);
  }
  [[nodiscard]] bool is_int() const { return std::holds_alternative<int>(value); }
  [[nodiscard]] bool is_bool() const {
    return std::holds_alternative<bool>(value);
  }
  [[nodiscard]] bool is_list() const {
    return std::holds_alternative<std::vector<DslValue>>(value);
  }
  [[nodiscard]] bool is_identifier() const {
    return std::holds_alternative<Identifier>(value);
  }

  [[nodiscard]] const std::string &get_string() const {
    return std::get<std::string>(value);
  }
  [[nodiscard]] int get_int() const { return std::get<int>(value); }
  [[nodiscard]] bool get_bool() const { return std::get<bool>(value); }
  [[nodiscard]] const std::vector<DslValue> &get_list() const {
    return std::get<std::vector<DslValue>>(value);
  }
  [[nodiscard]] const std::string &get_identifier() const {
    return std::get<Identifier>(value).name;
  }

  [[nodiscard]] std::string to_string() const;
};

struct Token {
  enum class Type {
    LET,
    ASSIGN,
    COLON_ASSIGN,
    SEMICOLON,
    COMMA,
    DOT,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    IDENTIFIER,
    STRING,
    NUMBER,
    BOOL,
    END_OF_FILE
  };

  Type type{Type::END_OF_FILE};
  std::string value;
  int line{1};
  int column{1};
};

class Lexer {
public:
  explicit Lexer(std::string src) : src_(std::move(src)) {}
  [[nodiscard]] Token next_token();

private:
  std::string src_;
  size_t pos_{0};
  int line_{1};
  int col_{1};

  [[nodiscard]] char peek() const noexcept;
  char advance() noexcept;
  void skip_whitespace_and_comments() noexcept;
};

class DslParser {
public:
  [[nodiscard]] absl::flat_hash_map<std::string, ScraperBuilder>
  parse(const std::string &dsl_source);

  [[nodiscard]] absl::flat_hash_map<std::string, ScraperBuilder>
  parse_file(const std::string &filepath);

private:
  Lexer *lexer_{nullptr};
  Token current_;
  absl::flat_hash_map<std::string, DslValue> variables_;
  absl::flat_hash_map<std::string, ScraperBuilder> scrapers_;

  void advance_token();
  void expect(Token::Type type, std::string_view msg);
  [[nodiscard]] DslValue parse_arg(int depth = 0);
  [[nodiscard]] std::vector<DslValue> parse_arg_list(int depth = 0);
  void parse_chain(ScraperBuilder &builder, int depth = 0);
  void apply_method(ScraperBuilder &builder, std::string_view name,
                    const std::vector<DslValue> &args);
};

} // namespace scraper::dsl
