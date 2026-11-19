#pragma once

#include "digger/token.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace digger {

inline constexpr size_t MAX_PARSE_DEPTH = 512;

/**
 * Tolerant lexer for JavaScript / JSON-like snippets.
 *
 * Strips single-line and block comments, filters JS control keywords, decodes
 * quoted/template string literals (including \xHH, \uHHHH with surrogate pairs,
 * and \u{H...}), and lexes numeric literals (including negative and scientific
 * notation) as TokenType::Number.
 */
class TokenParser {
public:
  using token_vector = std::vector<Token>;

  TokenParser() = default;

  void init(std::string_view source) noexcept;
  [[nodiscard]] token_vector parse(std::string_view source);
  [[nodiscard]] bool next_token(Token &out_token);

private:
  enum class KeywordAction { ExpectNextIdentifier, Ignore };

  static constexpr std::array<std::pair<std::string_view, KeywordAction>, 42>
      kKeywordActionMap = {{{"async", KeywordAction::Ignore},
                            {"break", KeywordAction::Ignore},
                            {"case", KeywordAction::Ignore},
                            {"catch", KeywordAction::Ignore},
                            {"class", KeywordAction::Ignore},
                            {"const", KeywordAction::ExpectNextIdentifier},
                            {"continue", KeywordAction::Ignore},
                            {"debugger", KeywordAction::Ignore},
                            {"default", KeywordAction::Ignore},
                            {"delete", KeywordAction::Ignore},
                            {"do", KeywordAction::Ignore},
                            {"else", KeywordAction::Ignore},
                            {"enum", KeywordAction::Ignore},
                            {"export", KeywordAction::Ignore},
                            {"extends", KeywordAction::Ignore},
                            {"finally", KeywordAction::Ignore},
                            {"for", KeywordAction::Ignore},
                            {"function", KeywordAction::ExpectNextIdentifier},
                            {"if", KeywordAction::Ignore},
                            {"import", KeywordAction::Ignore},
                            {"in", KeywordAction::Ignore},
                            {"instanceof", KeywordAction::Ignore},
                            {"interface", KeywordAction::Ignore},
                            {"let", KeywordAction::ExpectNextIdentifier},
                            {"new", KeywordAction::Ignore},
                            {"package", KeywordAction::Ignore},
                            {"private", KeywordAction::Ignore},
                            {"protected", KeywordAction::Ignore},
                            {"public", KeywordAction::Ignore},
                            {"return", KeywordAction::ExpectNextIdentifier},
                            {"static", KeywordAction::Ignore},
                            {"super", KeywordAction::Ignore},
                            {"switch", KeywordAction::Ignore},
                            {"this", KeywordAction::Ignore},
                            {"throw", KeywordAction::Ignore},
                            {"try", KeywordAction::Ignore},
                            {"typeof", KeywordAction::Ignore},
                            {"var", KeywordAction::ExpectNextIdentifier},
                            {"void", KeywordAction::Ignore},
                            {"while", KeywordAction::Ignore},
                            {"with", KeywordAction::Ignore},
                            {"yield", KeywordAction::Ignore}}};

  std::string_view source_;
  size_t pos_{0};
  bool expect_next_identifier_{false};

  void skip_whitespace_and_comments() noexcept;
  void emit_single_char(char c, TokenType type, Token &out_token);
  [[nodiscard]] bool starts_number_at(size_t idx) const noexcept;
  void handle_number_token(Token &out_token);
  void handle_string_token(Token &out_token);
  [[nodiscard]] bool handle_identifier_or_keyword(Token &out_token);
  void decode_escape_sequence(std::string &out);
};

void tokenize_and_simplify(std::string_view source,
                           TokenParser::token_vector &out_tokens,
                           size_t depth = 0);

} // namespace digger
