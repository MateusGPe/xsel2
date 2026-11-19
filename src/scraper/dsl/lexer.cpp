#include "scraper/dsl/lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace scraper::dsl {

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
      while (pos_ < src_.size() && peek() != '\n') {
        advance();
      }
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
      while (pos_ < src_.size() && peek() != '\n') {
        advance();
      }
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
  auto single = [&](Token::Type t, const char *lit) -> Token {
    advance();
    return {t, lit, tok_line, tok_col};
  };

  switch (c) {
  case ';':
    return single(Token::Type::SEMICOLON, ";");
  case ',':
    return single(Token::Type::COMMA, ",");
  case '.':
    return single(Token::Type::DOT, ".");
  case '(':
    return single(Token::Type::LPAREN, "(");
  case ')':
    return single(Token::Type::RPAREN, ")");
  case '{':
    return single(Token::Type::LBRACE, "{");
  case '}':
    return single(Token::Type::RBRACE, "}");
  case '[':
    return single(Token::Type::LBRACKET, "[");
  case ']':
    return single(Token::Type::RBRACKET, "]");
  case '=':
    return single(Token::Type::ASSIGN, "=");
  case '$':
    return single(Token::Type::DOLLAR, "$");
  case '@':
    return single(Token::Type::AT, "@");
  case ':':
    advance();
    if (peek() == '=') {
      advance();
      return {Token::Type::COLON_ASSIGN, ":=", tok_line, tok_col};
    }
    return {Token::Type::COLON, ":", tok_line, tok_col};
  case '|':
    advance();
    if (peek() == '>') {
      advance();
      return {Token::Type::PIPE, "|>", tok_line, tok_col};
    }
    return {Token::Type::PIPE, "|", tok_line, tok_col};
  default:
    break;
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
    if (c == '-') {
      num += advance();
    }
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
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
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
    if (ident == "let") {
      return {Token::Type::LET, ident, tok_line, tok_col};
    }
    if (ident == "true" || ident == "false") {
      return {Token::Type::BOOL, ident, tok_line, tok_col};
    }
    if (ident == "null") {
      return {Token::Type::NULL_LIT, ident, tok_line, tok_col};
    }
    return {Token::Type::IDENTIFIER, std::move(ident), tok_line, tok_col};
  }

  throw std::runtime_error("Unexpected character in DSL at line " +
                           std::to_string(tok_line) + ": '" +
                           std::string(1, c) + "'");
}

} // namespace scraper::dsl
