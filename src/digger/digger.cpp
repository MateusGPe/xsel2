#include "digger/digger.hpp"
#include "common/json_utils.hpp"
#include "digger/scalar_parser.hpp"
#include <optional>
#include <string>
#include <vector>

namespace digger {

namespace {

struct BuildFrame {
  jsoncons::json node;
  bool is_map{false};
  bool expecting_key{false};
  std::optional<std::string> pending_key;
  std::string key_in_parent;
};

jsoncons::json make_container(bool as_map) {
  if (as_map) {
    return jsoncons::json::object();
  }
  return jsoncons::json::array();
}

void flush_top_frame(std::vector<BuildFrame> &frames) {
  if (frames.size() <= 1) {
    return;
  }
  BuildFrame child = std::move(frames.back());
  frames.pop_back();
  BuildFrame &parent = frames.back();
  if (parent.node.is_array()) {
    parent.node.push_back(std::move(child.node));
  } else if (parent.node.is_object()) {
    std::string key = xsel::common::next_unique_key(
        parent.node,
        child.key_in_parent.empty() ? "_item" : child.key_in_parent);
    parent.node.insert_or_assign(std::move(key), std::move(child.node));
  }
}

void handle_start(const Token &token, std::vector<BuildFrame> &frames,
                  size_t &virtual_depth, size_t &overflow_depth) {
  if (overflow_depth > 0 || frames.size() >= MAX_PARSE_DEPTH) {
    ++overflow_depth;
    ++virtual_depth;
    return;
  }

  const char start_char = token.text.empty() ? '{' : token.text.front();
  const bool child_is_map = (start_char == '{');
  BuildFrame &top = frames.back();

  if (top.node.is_null()) {
    top.node = make_container(child_is_map);
    top.is_map = child_is_map;
    top.expecting_key = child_is_map;
    top.pending_key.reset();
    ++virtual_depth;
    return;
  }

  if (top.node.is_array()) {
    frames.push_back({make_container(child_is_map), child_is_map, child_is_map,
                      std::nullopt, ""});
    ++virtual_depth;
    return;
  }

  if (top.node.is_object()) {
    if (!top.pending_key.has_value()) {
      ++overflow_depth;
      ++virtual_depth;
      return;
    }
    std::string key =
        xsel::common::next_unique_key(top.node, *top.pending_key);
    top.pending_key.reset();
    top.expecting_key = true;
    frames.push_back({make_container(child_is_map), child_is_map, child_is_map,
                      std::nullopt, std::move(key)});
    ++virtual_depth;
    return;
  }

  ++overflow_depth;
  ++virtual_depth;
}

void handle_scalar(const Token &token, BuildFrame &top) {
  if (top.node.is_null()) {
    top.node = jsoncons::json::object();
    top.is_map = true;
    top.expecting_key = true;
  }

  if (top.is_map && top.node.is_object()) {
    if (top.expecting_key) {
      top.pending_key = token.text;
      top.expecting_key = false;
    } else if (top.pending_key.has_value()) {
      std::string key =
          xsel::common::next_unique_key(top.node, *top.pending_key);
      top.pending_key.reset();
      top.node.insert_or_assign(std::move(key), parse_scalar(token));
      top.expecting_key = true;
    } else {
      std::string key = xsel::common::next_unique_key(top.node, "_value");
      top.node.insert_or_assign(std::move(key), parse_scalar(token));
      top.expecting_key = true;
    }
    return;
  }

  if (top.node.is_array()) {
    top.node.push_back(parse_scalar(token));
    top.expecting_key = false;
    return;
  }
}

void handle_operator(const Token &token, BuildFrame &top) {
  if (!top.is_map) {
    return;
  }
  if (token.text == "=" || token.text == ":") {
    top.expecting_key = false;
  } else if (token.text == "," || token.text == ";") {
    top.pending_key.reset();
    top.expecting_key = true;
  }
}

void build_json_from_tokens(const TokenParser::token_vector &tokens,
                            jsoncons::json &out) {
  std::vector<BuildFrame> frames;
  frames.reserve(16);
  const bool root_is_map = out.is_object();
  frames.push_back({std::move(out), root_is_map, root_is_map, std::nullopt, ""});

  size_t virtual_depth = 1;
  size_t overflow_depth = 0;

  for (const Token &token : tokens) {
    if (frames.empty()) {
      break;
    }
    switch (token.type) {
    case TokenType::Start:
      handle_start(token, frames, virtual_depth, overflow_depth);
      break;
    case TokenType::Identifier:
    case TokenType::String:
    case TokenType::Number:
      if (overflow_depth == 0) {
        handle_scalar(token, frames.back());
      }
      break;
    case TokenType::Operator:
      if (overflow_depth == 0) {
        handle_operator(token, frames.back());
      }
      break;
    case TokenType::End:
      if (overflow_depth > 0) {
        --overflow_depth;
        --virtual_depth;
        break;
      }
      if (virtual_depth <= 1) {
        while (frames.size() > 1) {
          flush_top_frame(frames);
        }
        out = std::move(frames.front().node);
        return;
      }
      flush_top_frame(frames);
      --virtual_depth;
      break;
    }
  }

  while (frames.size() > 1) {
    flush_top_frame(frames);
  }
  out = std::move(frames.front().node);
}

} // namespace

jsoncons::json parse_object(std::string_view source, bool simplify) {
  jsoncons::json out = jsoncons::json::null();
  parse_object_into(source, out, simplify);
  return out;
}

void parse_object_into(std::string_view source, jsoncons::json &out,
                       bool simplify) {
  TokenParser::token_vector tokens;
  if (simplify) {
    tokens.reserve(source.size() / 4);
    tokenize_and_simplify(source, tokens);
  } else {
    TokenParser parser;
    tokens = parser.parse(source);
  }
  build_json_from_tokens(tokens, out);
}

} // namespace digger
