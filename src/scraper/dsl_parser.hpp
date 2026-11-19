#pragma once

#include "scraper/scraper.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scraper::dsl {

inline constexpr int MAX_DSL_DEPTH = 256;

struct DslValue {
  struct Identifier {
    std::string name;
  };

  using ObjectType = std::vector<std::pair<std::string, DslValue>>;
  using ValueType =
      std::variant<std::monostate, std::string, int, double, bool,
                   std::vector<DslValue>, ObjectType, Identifier>;

  ValueType value;

  DslValue() : value(std::monostate{}) {}
  explicit DslValue(std::monostate) : value(std::monostate{}) {}
  explicit DslValue(std::string s) : value(std::move(s)) {}
  explicit DslValue(int i) : value(i) {}
  explicit DslValue(double d) : value(d) {}
  explicit DslValue(bool b) : value(b) {}
  explicit DslValue(std::vector<DslValue> l) : value(std::move(l)) {}
  explicit DslValue(ObjectType obj) : value(std::move(obj)) {}
  DslValue(std::string name, bool is_ident);

  [[nodiscard]] bool is_null() const {
    return std::holds_alternative<std::monostate>(value);
  }
  [[nodiscard]] bool is_string() const {
    return std::holds_alternative<std::string>(value);
  }
  [[nodiscard]] bool is_int() const { return std::holds_alternative<int>(value); }
  [[nodiscard]] bool is_double() const {
    return std::holds_alternative<double>(value);
  }
  [[nodiscard]] bool is_bool() const {
    return std::holds_alternative<bool>(value);
  }
  [[nodiscard]] bool is_list() const {
    return std::holds_alternative<std::vector<DslValue>>(value);
  }
  [[nodiscard]] bool is_object() const {
    return std::holds_alternative<ObjectType>(value);
  }
  [[nodiscard]] bool is_identifier() const {
    return std::holds_alternative<Identifier>(value);
  }

  [[nodiscard]] const std::string &get_string() const {
    return std::get<std::string>(value);
  }
  [[nodiscard]] int get_int() const { return std::get<int>(value); }
  [[nodiscard]] double get_double() const { return std::get<double>(value); }
  [[nodiscard]] bool get_bool() const { return std::get<bool>(value); }
  [[nodiscard]] const std::vector<DslValue> &get_list() const {
    return std::get<std::vector<DslValue>>(value);
  }
  [[nodiscard]] const ObjectType &get_object() const {
    return std::get<ObjectType>(value);
  }
  [[nodiscard]] const std::string &get_identifier() const {
    return std::get<Identifier>(value).name;
  }

  [[nodiscard]] std::string to_string() const;
};

struct DslArgList {
  std::vector<DslValue> positional;
  absl::flat_hash_map<std::string, DslValue> named;
};

struct Token {
  enum class Type {
    LET,
    ASSIGN,
    COLON,
    COLON_ASSIGN,
    SEMICOLON,
    COMMA,
    DOT,
    PIPE,
    ARROW,
    DOUBLE_ARROW,
    DOLLAR,
    AT,
    DOUBLE_QUESTION,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    IDENTIFIER,
    STRING,
    NUMBER,
    FLOAT,
    BOOL,
    NULL_LIT,
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
  [[nodiscard]] std::string interpolate_string(const std::string &raw) const;
  [[nodiscard]] DslValue parse_arg(int depth = 0);
  [[nodiscard]] DslArgList parse_arg_list(int depth = 0);
  void apply_method(ScraperBuilder &builder, std::string_view name,
                    const DslArgList &args);
  void parse_chain(ScraperBuilder &builder, int depth = 0,
                   bool in_block = false);
};

} // namespace scraper::dsl
