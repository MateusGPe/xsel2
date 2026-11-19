#include "digger/tokenizer.hpp"
#include "digger/scalar_parser.hpp"
#include <algorithm>
#include <string>

namespace digger {

namespace {

inline constexpr bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

inline constexpr bool is_alnum(char c) noexcept {
  const auto uc = static_cast<unsigned char>(c);
  return (uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') ||
         (uc >= '0' && uc <= '9');
}

inline constexpr bool is_identifier_char(char c) noexcept {
  return is_alnum(c) || c == '_' || c == '$' || c == '.';
}

inline constexpr int hex_digit_to_int(char c) noexcept {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

inline void append_utf8_codepoint(std::string &out, uint32_t cp) {
  if (cp <= 0x7F) {
    out += static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    out += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    out += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0x10FFFF) {
    out += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += '?';
  }
}

inline constexpr bool are_matching_brackets(std::string_view start,
                                            std::string_view end) noexcept {
  if (start.length() == 1 && end.length() == 1) {
    const char s = start[0], e = end[0];
    return (s == '(' && e == ')') || (s == '[' && e == ']') ||
           (s == '{' && e == '}');
  }
  return false;
}

} // namespace

void TokenParser::init(std::string_view source) noexcept {
  source_ = source;
  pos_ = 0;
  expect_next_identifier_ = false;
}

TokenParser::token_vector TokenParser::parse(std::string_view source) {
  init(source);
  token_vector tokens;
  tokens.reserve(source.size() / 4);
  Token tok;
  while (next_token(tok)) {
    tokens.push_back(std::move(tok));
  }
  return tokens;
}

void TokenParser::skip_whitespace_and_comments() noexcept {
  while (pos_ < source_.size()) {
    if (detail::is_space_char(source_[pos_])) {
      ++pos_;
      continue;
    }
    if (pos_ + 1 < source_.size() && source_[pos_] == '/') {
      if (source_[pos_ + 1] == '/') {
        pos_ += 2;
        while (pos_ < source_.size() && source_[pos_] != '\n') {
          ++pos_;
        }
        continue;
      }
      if (source_[pos_ + 1] == '*') {
        pos_ += 2;
        while (pos_ + 1 < source_.size() &&
               !(source_[pos_] == '*' && source_[pos_ + 1] == '/')) {
          ++pos_;
        }
        pos_ = (pos_ + 1 < source_.size()) ? pos_ + 2 : source_.size();
        continue;
      }
    }
    break;
  }
}

void TokenParser::emit_single_char(char c, TokenType type, Token &out_token) {
  out_token.text.assign(1, c);
  out_token.type = type;
  ++pos_;
}

bool TokenParser::starts_number_at(size_t idx) const noexcept {
  if (idx >= source_.size()) {
    return false;
  }
  const char c = source_[idx];
  if (is_digit(c)) {
    return true;
  }
  if (c == '.' && idx + 1 < source_.size() && is_digit(source_[idx + 1])) {
    return true;
  }
  if (c == '-' && idx + 1 < source_.size()) {
    const char next = source_[idx + 1];
    if (is_digit(next)) {
      return true;
    }
    if (next == '.' && idx + 2 < source_.size() && is_digit(source_[idx + 2])) {
      return true;
    }
  }
  return false;
}

void TokenParser::handle_number_token(Token &out_token) {
  const size_t start = pos_;
  if (source_[pos_] == '-') {
    ++pos_;
  }
  while (pos_ < source_.size() && is_digit(source_[pos_])) {
    ++pos_;
  }
  if (pos_ < source_.size() && source_[pos_] == '.') {
    ++pos_;
    while (pos_ < source_.size() && is_digit(source_[pos_])) {
      ++pos_;
    }
  }
  if (pos_ < source_.size() &&
      (source_[pos_] == 'e' || source_[pos_] == 'E')) {
    size_t exp_pos = pos_ + 1;
    if (exp_pos < source_.size() &&
        (source_[exp_pos] == '+' || source_[exp_pos] == '-')) {
      ++exp_pos;
    }
    if (exp_pos < source_.size() && is_digit(source_[exp_pos])) {
      pos_ = exp_pos;
      while (pos_ < source_.size() && is_digit(source_[pos_])) {
        ++pos_;
      }
    }
  }
  out_token.type = TokenType::Number;
  out_token.text.assign(source_.substr(start, pos_ - start));
  expect_next_identifier_ = false;
}

bool TokenParser::parse_four_hex_digits(size_t start_pos,
                                        uint32_t &out_value) const noexcept {
  if (start_pos + 4 > source_.size()) {
    return false;
  }
  uint32_t val = 0;
  for (size_t i = 0; i < 4; ++i) {
    const int d = hex_digit_to_int(source_[start_pos + i]);
    if (d < 0) {
      return false;
    }
    val = (val << 4) | static_cast<uint32_t>(d);
  }
  out_value = val;
  return true;
}

void TokenParser::decode_escape_sequence(std::string &out) {
  ++pos_; // consume '\\'
  if (pos_ >= source_.size()) {
    out += '\\';
    return;
  }
  const char esc = source_[pos_];
  switch (esc) {
  case 'n':
    out += '\n';
    ++pos_;
    break;
  case 'r':
    out += '\r';
    ++pos_;
    break;
  case 't':
    out += '\t';
    ++pos_;
    break;
  case 'b':
    out += '\b';
    ++pos_;
    break;
  case 'f':
    out += '\f';
    ++pos_;
    break;
  case 'v':
    out += '\v';
    ++pos_;
    break;
  case '0':
    out += '\0';
    ++pos_;
    break;
  case '\\':
  case '"':
  case '\'':
  case '`':
  case '/':
    out += esc;
    ++pos_;
    break;
  case 'x': {
    if (pos_ + 2 < source_.size()) {
      const int hi = hex_digit_to_int(source_[pos_ + 1]);
      const int lo = hex_digit_to_int(source_[pos_ + 2]);
      if (hi >= 0 && lo >= 0) {
        out += static_cast<char>((hi << 4) | lo);
        pos_ += 3;
        return;
      }
    }
    out += 'x';
    ++pos_;
    break;
  }
  case 'u': {
    if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '{') {
      size_t hex_pos = pos_ + 2;
      uint32_t cp = 0;
      size_t digits = 0;
      while (hex_pos < source_.size() && source_[hex_pos] != '}' &&
             digits < 6) {
        const int d = hex_digit_to_int(source_[hex_pos]);
        if (d < 0)
          break;
        cp = (cp << 4) | static_cast<uint32_t>(d);
        ++hex_pos;
        ++digits;
      }
      if (digits > 0 && hex_pos < source_.size() && source_[hex_pos] == '}') {
        append_utf8_codepoint(out, cp);
        pos_ = hex_pos + 1;
        return;
      }
    }
    uint32_t cp = 0;
    if (parse_four_hex_digits(pos_ + 1, cp)) {
      pos_ += 5;
      if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= source_.size() &&
          source_[pos_] == '\\' && source_[pos_ + 1] == 'u') {
        uint32_t low = 0;
        if (parse_four_hex_digits(pos_ + 2, low) && low >= 0xDC00 &&
            low <= 0xDFFF) {
          const uint32_t combined =
              0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
          append_utf8_codepoint(out, combined);
          pos_ += 6;
          return;
        }
      }
      append_utf8_codepoint(out, cp);
      return;
    }
    out += 'u';
    ++pos_;
    break;
  }
  default:
    out += esc;
    ++pos_;
    break;
  }
}

void TokenParser::handle_string_token(Token &out_token) {
  const char quote = source_[pos_++];
  out_token.type = TokenType::String;
  out_token.text.clear();

  while (pos_ < source_.size()) {
    const char c = source_[pos_];
    if (c == quote) {
      ++pos_;
      expect_next_identifier_ = false;
      return;
    }
    if (c == '\\') {
      decode_escape_sequence(out_token.text);
    } else {
      out_token.text += c;
      ++pos_;
    }
  }
  expect_next_identifier_ = false;
}

bool TokenParser::handle_identifier_or_keyword(Token &out_token) {
  const size_t start = pos_;
  while (pos_ < source_.size() && is_identifier_char(source_[pos_])) {
    ++pos_;
  }
  const std::string_view word = source_.substr(start, pos_ - start);

  if (expect_next_identifier_) {
    expect_next_identifier_ = false;
    out_token.type = TokenType::Identifier;
    out_token.text.assign(word);
    return true;
  }

  // Binary search in sorted keyword table (not constexpr-sorted by string?
  // Let's check: kKeywordActionMap in header was mostly sorted, except "async"
  // at the end! So let's use std::find_if or ensure it's sorted!).
  for (const auto &[kw, action] : kKeywordActionMap) {
    if (kw == word) {
      if (action == KeywordAction::ExpectNextIdentifier) {
        expect_next_identifier_ = true;
      }
      return false;
    }
  }

  out_token.type = TokenType::Identifier;
  out_token.text.assign(word);
  return true;
}

bool TokenParser::next_token(Token &out_token) {
  while (pos_ < source_.size()) {
    skip_whitespace_and_comments();
    if (pos_ >= source_.size()) {
      return false;
    }

    const char c = source_[pos_];
    if (c == '{' || c == '[' || c == '(') {
      emit_single_char(c, TokenType::Start, out_token);
      expect_next_identifier_ = false;
      return true;
    }
    if (c == '}' || c == ']' || c == ')') {
      emit_single_char(c, TokenType::End, out_token);
      expect_next_identifier_ = false;
      return true;
    }
    if (c == '"' || c == '\'' || c == '`') {
      handle_string_token(out_token);
      return true;
    }
    if (c == ':' || c == '=' || c == ',' || c == ';' || c == '+') {
      emit_single_char(c, TokenType::Operator, out_token);
      expect_next_identifier_ = false;
      return true;
    }
    if (starts_number_at(pos_)) {
      handle_number_token(out_token);
      return true;
    }
    if (is_identifier_char(c)) {
      if (handle_identifier_or_keyword(out_token)) {
        return true;
      }
      continue;
    }
    ++pos_;
  }
  return false;
}

void tokenize_and_simplify(std::string_view source,
                           TokenParser::token_vector &st, size_t depth) {
  if (depth >= MAX_PARSE_DEPTH) {
    return;
  }

  TokenParser parser;
  parser.init(source);

  auto reduce_stack = [&st, depth]() {
    bool reduced = true;
    while (reduced && st.size() >= 2) {
      reduced = false;
      const size_t n = st.size();

      // Rule 1: Join string literals ("a" + "b" -> "ab")
      if (n >= 3 && st[n - 3].type == TokenType::String &&
          st[n - 2].type == TokenType::Operator && st[n - 2].text == "+" &&
          st[n - 1].type == TokenType::String) {
        st[n - 3].text += st[n - 1].text;
        st.pop_back();
        st.pop_back();
        reduced = true;
        continue;
      }

      // Rule 2: Unwrap single inner element ((x) -> x)
      if (n >= 3 && st[n - 3].type == TokenType::Start &&
          st[n - 1].type == TokenType::End &&
          are_matching_brackets(st[n - 3].text, st[n - 1].text)) {
        const TokenType mid_type = st[n - 2].type;
        if (mid_type == TokenType::Identifier ||
            mid_type == TokenType::Operator || mid_type == TokenType::String ||
            mid_type == TokenType::Number) {
          Token mid = std::move(st[n - 2]);
          st.pop_back();
          st.pop_back();
          st.pop_back();
          st.push_back(std::move(mid));
          reduced = true;
          continue;
        }
      }

      // Rule 3: Remove empty matching brackets (), [], {}
      if (st[n - 2].type == TokenType::Start &&
          st[n - 1].type == TokenType::End &&
          are_matching_brackets(st[n - 2].text, st[n - 1].text)) {
        st.pop_back();
        st.pop_back();
        reduced = true;
        continue;
      }

      // Rule 4: Inline JSON.parse("...") evaluation
      if (st[n - 2].text == "JSON.parse" &&
          (st[n - 2].type == TokenType::Identifier ||
           st[n - 2].type == TokenType::String) &&
          st[n - 1].type == TokenType::String) {
        std::string json_str = std::move(st[n - 1].text);
        st.pop_back();
        st.pop_back();
        tokenize_and_simplify(json_str, st, depth + 1);
        reduced = true;
        continue;
      }
    }
  };

  Token t;
  while (parser.next_token(t)) {
    st.push_back(std::move(t));
    reduce_stack();
  }
}

} // namespace digger
