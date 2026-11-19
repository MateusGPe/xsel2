#pragma once

#include <string>

namespace digger {

enum class TokenType { Start, String, Number, Identifier, Operator, End };

struct Token {
  TokenType type{TokenType::Identifier};
  std::string text;
};

} // namespace digger
