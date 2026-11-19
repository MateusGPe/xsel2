#include "scraper/steps/regex_steps.hpp"
#include "scraper/steps/text_steps.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace scraper {

namespace detail {

re2::RE2::Options get_re2_options(const std::optional<std::string> &flags_opt) {
  re2::RE2::Options opts;
  opts.set_log_errors(false);
  if (!flags_opt.has_value() || flags_opt->empty()) {
    opts.set_case_sensitive(false);
    return opts;
  }
  const std::string &flags = *flags_opt;
  opts.set_case_sensitive(flags.find("icase") == std::string::npos);
  if (flags.find("dotall") != std::string::npos) {
    opts.set_dot_nl(true);
  }
  if (flags.find("multiline") != std::string::npos) {
    opts.set_one_line(false);
  }
  return opts;
}

} // namespace detail

namespace {

void validate_regex(const re2::RE2 &regex, std::string_view context,
                    std::string_view pattern) {
  if (!regex.ok()) {
    throw std::invalid_argument(std::string(context) + ": invalid regex '" +
                                std::string(pattern) + "': " + regex.error());
  }
}

} // namespace

RegexReplaceStep::RegexReplaceStep(std::string_view pattern,
                                   std::string_view replacement,
                                   const std::optional<std::string> &flags)
    : pattern_str_(pattern),
      regex_(pattern_str_, detail::get_re2_options(flags)),
      replacement_str_(replacement) {
  validate_regex(regex_, "regex_replace", pattern_str_);
}

void RegexReplaceStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &) {
  for (auto &value : values) {
    re2::RE2::GlobalReplace(&value, regex_, replacement_str_);
  }
}

RegexFilterStep::RegexFilterStep(std::string_view pattern, bool negate,
                                 const std::optional<std::string> &flags)
    : pattern_str_(pattern),
      regex_(pattern_str_, detail::get_re2_options(flags)),
      negate_match_(negate) {
  validate_regex(regex_, "regex_filter", pattern_str_);
}

void RegexFilterStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &) {
  values.erase(std::remove_if(values.begin(), values.end(),
                              [this](const std::string &v) {
                                const bool is_match =
                                    re2::RE2::PartialMatch(v, regex_);
                                return negate_match_ ? is_match : !is_match;
                              }),
               values.end());
}

RegexExtractStep::RegexExtractStep(std::string_view pattern, int group_index,
                                   const std::optional<std::string> &flags)
    : pattern_str_(pattern),
      regex_(pattern_str_, detail::get_re2_options(flags)),
      group_index_(group_index) {
  validate_regex(regex_, "regex_extract", pattern_str_);
}

void RegexExtractStep::apply(Node, std::vector<std::string> &values,
                             jsoncons::json &) {
  const int num_groups = regex_.NumberOfCapturingGroups();
  const int nsubmatch = num_groups + 1;
  std::vector<re2::StringPiece> match(static_cast<size_t>(nsubmatch));
  std::vector<std::string> new_values;

  for (const auto &value : values) {
    if (regex_.Match(value, 0, value.size(), re2::RE2::UNANCHORED, match.data(),
                     nsubmatch)) {
      if (group_index_ <= 0) {
        size_t start_group = 0;
        if (group_index_ < 0) {
          start_group = static_cast<size_t>(-static_cast<int64_t>(group_index_));
          if (start_group == 0)
            start_group = 1;
        }
        for (size_t i = start_group; i < static_cast<size_t>(nsubmatch); ++i) {
          if (match[i].data() != nullptr) {
            new_values.emplace_back(match[i]);
          }
        }
      } else {
        const size_t idx = static_cast<size_t>(group_index_);
        if (idx < static_cast<size_t>(nsubmatch) &&
            match[idx].data() != nullptr) {
          new_values.emplace_back(match[idx]);
        } else {
          new_values.emplace_back("");
        }
      }
    } else if (group_index_ >= 0) {
      new_values.emplace_back("");
    }
  }
  values = std::move(new_values);
}

FilterStep::FilterStep(const std::vector<std::string> &patterns) {
  if (patterns.empty()) {
    throw std::runtime_error("filter: pattern list cannot be empty.");
  }
  for (const auto &p : patterns) {
    if (p.empty())
      continue;
    if (p[0] == '+') {
      if (p.size() > 1) {
        include_ac_.AddPattern(p.substr(1));
        has_include_ = true;
      }
    } else if (p[0] == '-') {
      if (p.size() > 1) {
        exclude_ac_.AddPattern(p.substr(1));
        has_exclude_ = true;
      }
    } else {
      include_ac_.AddPattern(p);
      has_include_ = true;
    }
  }
  if (has_include_)
    include_ac_.Compile();
  if (has_exclude_)
    exclude_ac_.Compile();
}

void FilterStep::apply(Node, std::vector<std::string> &values,
                       jsoncons::json &) {
  values.erase(std::remove_if(values.begin(), values.end(),
                              [this](const std::string &v) {
                                if (has_exclude_ &&
                                    exclude_ac_.HasAnyPattern(v)) {
                                  return true;
                                }
                                if (has_include_ &&
                                    !include_ac_.HasAnyPattern(v)) {
                                  return true;
                                }
                                return false;
                              }),
               values.end());
}

} // namespace scraper
