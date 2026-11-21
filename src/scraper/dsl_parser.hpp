#pragma once

#include "scraper/dsl/lexer.hpp"
#include "scraper/dsl/value.hpp"
#include "scraper/scraper.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <string_view>

namespace scraper::dsl {

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
  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<Step>>> pipes_;

  void advance_token();
  void expect(Token::Type type, std::string_view msg);
  [[nodiscard]] std::string interpolate_string(const std::string &raw) const;
  [[nodiscard]] DslValue parse_arg(int depth = 0);
  [[nodiscard]] DslArgList parse_arg_list(int depth = 0);
  void apply_method(ScraperBuilder &builder, std::string_view name,
                    const DslArgList &args);
  void parse_chain(ScraperBuilder &builder, int depth = 0,
                   bool in_block = false, bool in_brace_block = false);
};

} // namespace scraper::dsl
