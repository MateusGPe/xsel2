#include "scraper/steps/block_steps.hpp"
#include "digger/digger.hpp"

namespace scraper {

namespace detail {

std::string block_seeker_extract(std::string_view content,
                                 std::string_view prefix) {
  size_t search_start = 0;
  if (!prefix.empty()) {
    const size_t pos = content.find(prefix);
    if (pos == std::string_view::npos) {
      return {};
    }
    search_start = pos + prefix.size();
  }

  size_t open_pos = std::string_view::npos;
  char open_ch = '\0';
  char close_ch = '\0';
  for (size_t i = search_start; i < content.size(); ++i) {
    const char c = content[i];
    if (c == '{' || c == '[' || c == '(') {
      open_pos = i;
      open_ch = c;
      close_ch = (c == '{') ? '}' : (c == '[') ? ']' : ')';
      break;
    }
  }
  if (open_pos == std::string_view::npos) {
    return {};
  }

  int depth = 0;
  char quote = '\0';
  for (size_t i = open_pos; i < content.size(); ++i) {
    const char c = content[i];
    if (quote != '\0') {
      if (c == '\\' && i + 1 < content.size()) {
        ++i;
      } else if (c == quote) {
        quote = '\0';
      }
      continue;
    }
    if (c == '"' || c == '\'' || c == '`') {
      quote = c;
      continue;
    }
    if (c == open_ch) {
      ++depth;
    } else if (c == close_ch) {
      --depth;
      if (depth == 0) {
        return std::string(content.substr(open_pos, i - open_pos + 1));
      }
    }
  }
  return {};
}

} // namespace detail

void BlockSeekerStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &) {
  std::vector<std::string> out;
  out.reserve(values.size());
  for (const auto &v : values) {
    std::string block = detail::block_seeker_extract(v, prefix_str_);
    if (!block.empty()) {
      out.push_back(std::move(block));
    }
  }
  values = std::move(out);
}

void ParseStep::apply(Node, std::vector<std::string> &values,
                      jsoncons::json &dict) {
  if (values.empty()) {
    return;
  }

  std::string combined;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0)
      combined.push_back('\n');
    combined += values[i];
  }
  values.clear();

  jsoncons::json parsed = digger::parse_object(combined, simplify_);
  if (xsel::common::is_empty_json(parsed) && !opts_.allow_empty) {
    return;
  }

  if (objname_.empty()) {
    merge_into(dict, std::move(parsed), opts_.mode);
  } else {
    insert_into_json(dict, objname_, std::move(parsed), opts_);
  }
}

void BlockParserStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  jsoncons::json combined = jsoncons::json::object();

  for (const auto &v : values) {
    for (const auto &prefix : prefixes_) {
      std::string block = detail::block_seeker_extract(v, prefix);
      if (block.empty()) {
        continue;
      }
      jsoncons::json parsed = digger::parse_object(block, true);
      if (!xsel::common::is_empty_json(parsed)) {
        insert_into_json(combined, prefix, std::move(parsed),
                         InsertOptions{InsertMode::Merge, false, true});
      }
    }
  }

  values.clear();
  if (!combined.empty()) {
    merge_into(dict, std::move(combined));
  }
}

} // namespace scraper
