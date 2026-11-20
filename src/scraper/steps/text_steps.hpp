#pragma once

#include "scraper/step.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace scraper {

namespace detail {
void trim_in_place(std::string &value);
std::vector<std::string> split_string(std::string_view s,
                                      std::string_view delimiter,
                                      bool skip_empty_parts);
std::string url_unescape(std::string_view escaped_url);
} // namespace detail

class TrimStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class StripStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class LowerStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class UpperStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class ReplaceStep final : public Step {
public:
  ReplaceStep(std::string_view from_str, std::string_view to_str = "")
      : from_str_(from_str), to_str_(to_str) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string from_str_;
  std::string to_str_;
};

class SubstituteStep final : public Step {
public:
  SubstituteStep(std::string_view from_str, std::string_view to_str = "")
      : from_str_(from_str), to_str_(to_str) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string from_str_;
  std::string to_str_;
};

class SplitStep final : public Step {
public:
  explicit SplitStep(std::string_view delimiter, bool keep_empty_parts = false);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string delimiter_;
  bool skip_empty_parts_;
};

class JoinStep final : public Step {
public:
  explicit JoinStep(std::string_view delimiter) : delimiter_(delimiter) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string delimiter_;
};

class PrependStep final : public Step {
public:
  explicit PrependStep(std::string_view prefix_str) : prefix_str_(prefix_str) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string prefix_str_;
};

class AppendStep final : public Step {
public:
  explicit AppendStep(std::string_view suffix_str) : suffix_str_(suffix_str) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string suffix_str_;
};

class RemoveEmptyStep final : public Step {
public:
  explicit RemoveEmptyStep(bool also_trim = true) : also_trim_(also_trim) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool also_trim_;
};

class SubstringStep final : public Step {
public:
  SubstringStep(int start_pos, std::optional<int> length = std::nullopt)
      : start_pos_(start_pos), length_(length) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  int start_pos_;
  std::optional<int> length_;
};

class UnescapeStep final : public Step {
public:
  explicit UnescapeStep(bool url_decode = true) : url_decode_(url_decode) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool url_decode_;
};

class StripCommentsStep final : public Step {
public:
  StripCommentsStep(bool multiline = true, bool single_line = true)
      : multiline_(multiline), single_line_(single_line) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool multiline_;
  bool single_line_;
};

class StripTagsStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class Base64EncodeStep final : public Step {
public:
  explicit Base64EncodeStep(bool urlsafe = false) : urlsafe_(urlsafe) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool urlsafe_;
};

class Base64DecodeStep final : public Step {
public:
  explicit Base64DecodeStep(bool urlsafe = false) : urlsafe_(urlsafe) {}

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  bool urlsafe_;
};

class HashStep final : public Step {
public:
  enum class Algo { Fnv1a64, Sha256 };

  explicit HashStep(std::string_view algo = "fnv1a64");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  Algo algo_;
};

class UnicodeNormalizeStep final : public Step {
public:
  enum class Form { NFC, NFD, NFKC, NFKD };

  explicit UnicodeNormalizeStep(std::string_view form = "NFC");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  Form form_;
};

class StripAccentsStep final : public Step {
public:
  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;
};

class FuzzyFilterStep final : public Step {
public:
  FuzzyFilterStep(std::string_view query, double threshold = 0.8,
                  std::string_view algo = "jaro_winkler",
                  bool case_sensitive = false, bool sort_by_score = false);

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string query_;
  double threshold_;
  std::string algo_;
  bool case_sensitive_;
  bool sort_by_score_;
};

class SimilarityStep final : public Step {
public:
  SimilarityStep(std::string_view query, std::string_view algo = "jaro_winkler",
                 bool case_sensitive = false, std::string_view key = "");

  void apply(Node node, std::vector<std::string> &values,
             jsoncons::json &dict) override;

private:
  std::string query_;
  std::string algo_;
  bool case_sensitive_;
  std::string key_;
};

} // namespace scraper
