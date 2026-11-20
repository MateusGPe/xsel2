#include "digger/tokenizer.hpp"
#include "common/hex_utils.hpp"
#include "common/utf.hpp"
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
      const int hi = xsel::common::hex_nibble(source_[pos_ + 1]);
      const int lo = xsel::common::hex_nibble(source_[pos_ + 2]);
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
      while (hex_pos < source_.size() && source_[hex_pos] != '}') {
        const int d = xsel::common::hex_nibble(source_[hex_pos]);
        if (d < 0)
          break;
        cp = (cp > 0x10FFFFu) ? 0x110000u
                              : ((cp << 4) | static_cast<uint32_t>(d));
        ++hex_pos;
        ++digits;
      }
      if (digits > 0 && hex_pos < source_.size() && source_[hex_pos] == '}') {
        xsel::common::append_utf8_codepoint(out, cp);
        pos_ = hex_pos + 1;
        return;
      }
    }
    const int cp = xsel::common::parse_hex_4(source_, pos_ + 1);
    if (cp >= 0) {
      pos_ += 5;
      uint32_t codepoint = static_cast<uint32_t>(cp);
      if (codepoint >= 0xD800u && codepoint <= 0xDBFFu &&
          pos_ + 6 <= source_.size() && source_[pos_] == '\\' &&
          source_[pos_ + 1] == 'u') {
        const int low = xsel::common::parse_hex_4(source_, pos_ + 2);
        if (low >= 0xDC00 && low <= 0xDFFF) {
          codepoint = 0x10000u + (((codepoint - 0xD800u) << 10) |
                                  (static_cast<uint32_t>(low) - 0xDC00u));
          pos_ += 6;
        }
      }
      xsel::common::append_utf8_codepoint(out, codepoint);
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

  static_assert(std::is_sorted(kKeywordActionMap.begin(),
                               kKeywordActionMap.end(),
                               [](const auto &a, const auto &b) {
                                 return a.first < b.first;
                               }),
                "kKeywordActionMap must remain sorted for binary search");

  const auto it = std::lower_bound(
      kKeywordActionMap.begin(), kKeywordActionMap.end(), word,
      [](const auto &entry, std::string_view key) { return entry.first < key; });
  if (it != kKeywordActionMap.end() && it->first == word) {
    if (it->second == KeywordAction::ExpectNextIdentifier) {
      expect_next_identifier_ = true;
    }
    return false;
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
          st[n - 2].type == TokenType::Identifier &&
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
