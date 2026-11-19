#pragma once

#include <string>

namespace scraper::dsl {

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

} // namespace scraper::dsl
