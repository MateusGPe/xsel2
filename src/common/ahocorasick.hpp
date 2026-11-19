// File: common/ahocorasick.hpp
#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string_view>
#include <vector>

class AhoCorasick {
public:
  struct Match {
    std::size_t start;
    std::size_t length;
    uint32_t pattern_id;
  };

  using MatchList = std::vector<Match>;
  static constexpr uint32_t kInvalidState = std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t kInvalidPattern =
      std::numeric_limits<uint32_t>::max();

  AhoCorasick() { Clear(); }

  void Clear() {
    raw_trie_.clear();
    nodes_.clear();
    transitions_.clear();
    char_to_col_.fill(0u);
    alphabet_size_ = 1u; // Column 0 represents any byte absent from patterns
    num_patterns_ = 0u;
    is_compiled_ = false;

    // Temporary root node (state 0) for pattern insertion
    raw_trie_.emplace_back(0u);
  }

  // Inserts a pattern and returns its sequential pattern_id (0, 1, 2...).
  // Duplicate patterns return their original pattern_id.
  // Empty patterns are ignored and return kInvalidPattern.
  uint32_t AddPattern(std::string_view s) {
    assert(!is_compiled_ &&
           "Cannot add patterns after Compile() without Clear()");
    if (is_compiled_) {
      throw std::logic_error(
          "AhoCorasick: cannot add patterns after Compile() without Clear()");
    }
    if (s.empty()) {
      return kInvalidPattern;
    }

    uint32_t state = 0u;
    for (char ch : s) {
      const uint8_t uch = static_cast<uint8_t>(ch);
      uint32_t next_state = raw_trie_[state].children[uch];

      if (next_state == kInvalidState) {
        next_state = static_cast<uint32_t>(raw_trie_.size());
        raw_trie_[state].children[uch] = next_state;
        raw_trie_.emplace_back(raw_trie_[state].depth + 1u);
      }
      state = next_state;
    }

    if (raw_trie_[state].pattern_id == kInvalidPattern) {
      raw_trie_[state].pattern_id = num_patterns_++;
    }
    return raw_trie_[state].pattern_id;
  }

  // Compresses the alphabet, builds the flat 1D DFA via BFS, and frees the
  // temporary construction trie.
  void Compile() {
    if (is_compiled_) {
      return;
    }

    // 1. Identify bytes present in patterns and map them to dense columns 1..K.
    // Column 0 is reserved for all bytes not appearing in any pattern.
    std::array<bool, 256u> used_chars{};
    for (const auto &node : raw_trie_) {
      for (uint32_t ch = 0u; ch < 256u; ++ch) {
        if (node.children[ch] != kInvalidState) {
          used_chars[ch] = true;
        }
      }
    }

    std::vector<uint8_t> col_to_char;
    col_to_char.reserve(257u);
    col_to_char.push_back(0u); // Dummy entry for column 0
    char_to_col_.fill(0u);

    for (uint32_t ch = 0u; ch < 256u; ++ch) {
      if (used_chars[ch]) {
        char_to_col_[ch] = static_cast<uint16_t>(col_to_char.size());
        col_to_char.push_back(static_cast<uint8_t>(ch));
      }
    }
    alphabet_size_ = static_cast<uint32_t>(col_to_char.size());

    // 2. Allocate contiguous compressed DFA and copy trie structure.
    const uint32_t num_states = static_cast<uint32_t>(raw_trie_.size());
    nodes_.resize(num_states);
    transitions_.assign(
        static_cast<std::size_t>(num_states) * alphabet_size_, kInvalidState);

    for (uint32_t s = 0u; s < num_states; ++s) {
      nodes_[s].depth = raw_trie_[s].depth;
      nodes_[s].pattern_id = raw_trie_[s].pattern_id;

      const std::size_t row_offset = static_cast<std::size_t>(s) * alphabet_size_;
      for (uint32_t col = 1u; col < alphabet_size_; ++col) {
        transitions_[row_offset + col] =
            raw_trie_[s].children[col_to_char[col]];
      }
    }

    // Discard construction trie immediately to reclaim memory.
    raw_trie_.clear();
    raw_trie_.shrink_to_fit();

    // 3. Build failure links, output links, and complete DFA transitions via BFS.
    std::vector<uint32_t> failure_links(num_states, 0u);
    std::queue<uint32_t> q;

    transitions_[0] = 0u;

    for (uint32_t col = 1u; col < alphabet_size_; ++col) {
      const uint32_t next = transitions_[col];
      if (next != kInvalidState) {
        failure_links[next] = 0u;
        nodes_[next].next_output = 0u;
        nodes_[next].first_output =
            (nodes_[next].pattern_id != kInvalidPattern) ? next : 0u;
        q.push(next);
      } else {
        transitions_[col] = 0u;
      }
    }

    while (!q.empty()) {
      const uint32_t curr = q.front();
      q.pop();

      const uint32_t fail = failure_links[curr];
      const std::size_t curr_offset =
          static_cast<std::size_t>(curr) * alphabet_size_;
      const std::size_t fail_offset =
          static_cast<std::size_t>(fail) * alphabet_size_;

      // Any byte outside the pattern alphabet resets to root (state 0).
      transitions_[curr_offset] = 0u;

      for (uint32_t col = 1u; col < alphabet_size_; ++col) {
        const uint32_t next = transitions_[curr_offset + col];
        if (next != kInvalidState) {
          const uint32_t next_fail = transitions_[fail_offset + col];
          failure_links[next] = next_fail;
          const uint32_t next_out = nodes_[next_fail].first_output;
          nodes_[next].next_output = next_out;
          nodes_[next].first_output =
              (nodes_[next].pattern_id != kInvalidPattern) ? next : next_out;
          q.push(next);
        } else {
          transitions_[curr_offset + col] = transitions_[fail_offset + col];
        }
      }
    }

    is_compiled_ = true;
  }

  // Finds only the longest matching pattern ending at each position.
  bool FindAny(std::string_view text, MatchList &matches) const {
    EnsureCompiled();
    if (num_patterns_ == 0u) {
      return false;
    }

    bool found = false;
    uint32_t state = 0u;

    for (std::size_t i = 0u; i < text.size(); ++i) {
      state = Step(state, text[i]);
      const uint32_t out = nodes_[state].first_output;
      if (out != 0u) {
        const std::size_t length = nodes_[out].depth;
        matches.push_back(
            Match{i - length + 1u, length, nodes_[out].pattern_id});
        found = true;
      }
    }
    return found;
  }

  // Finds all occurrences, including overlapping suffix matches.
  bool Find(std::string_view text, MatchList &matches) const {
    EnsureCompiled();
    if (num_patterns_ == 0u) {
      return false;
    }

    bool found = false;
    uint32_t state = 0u;

    for (std::size_t i = 0u; i < text.size(); ++i) {
      state = Step(state, text[i]);
      for (uint32_t out = nodes_[state].first_output; out != 0u;
           out = nodes_[out].next_output) {
        const std::size_t length = nodes_[out].depth;
        matches.push_back(
            Match{i - length + 1u, length, nodes_[out].pattern_id});
        found = true;
      }
    }
    return found;
  }

  bool HasAnyPattern(std::string_view text) const {
    EnsureCompiled();
    if (num_patterns_ == 0u) {
      return false;
    }

    uint32_t state = 0u;
    for (char ch : text) {
      state = Step(state, ch);
      if (nodes_[state].first_output != 0u) {
        return true;
      }
    }
    return false;
  }

  bool HasAllPatterns(std::string_view text) const {
    EnsureCompiled();
    if (num_patterns_ == 0u) {
      return true;
    }

    // Allocation-free fast path for up to 64 distinct patterns.
    if (num_patterns_ <= 64u) {
      const uint64_t target_mask = (num_patterns_ == 64u)
                                       ? ~0ULL
                                       : ((1ULL << num_patterns_) - 1ULL);
      uint64_t found_mask = 0ULL;
      uint32_t state = 0u;

      for (char ch : text) {
        state = Step(state, ch);
        for (uint32_t out = nodes_[state].first_output; out != 0u;
             out = nodes_[out].next_output) {
          found_mask |= (1ULL << nodes_[out].pattern_id);
          if (found_mask == target_mask) {
            return true;
          }
        }
      }
      return false;
    }

    std::vector<uint8_t> found(num_patterns_, 0u);
    uint32_t found_count = 0u;
    uint32_t state = 0u;

    for (char ch : text) {
      state = Step(state, ch);
      for (uint32_t out = nodes_[state].first_output; out != 0u;
           out = nodes_[out].next_output) {
        const uint32_t pid = nodes_[out].pattern_id;
        if (!found[pid]) {
          found[pid] = 1u;
          if (++found_count == num_patterns_) {
            return true;
          }
        }
      }
    }
    return false;
  }

  bool IsEmpty() const noexcept { return num_patterns_ == 0u; }
  uint32_t PatternCount() const noexcept { return num_patterns_; }
  uint32_t AlphabetSize() const noexcept { return alphabet_size_; }
  bool IsCompiled() const noexcept { return is_compiled_; }

private:
  // Temporary trie node discarded after Compile().
  struct RawNode {
    uint32_t depth;
    uint32_t pattern_id;
    std::array<uint32_t, 256u> children;

    explicit RawNode(uint32_t d) : depth(d), pattern_id(kInvalidPattern) {
      children.fill(kInvalidState);
    }
  };

  // Compact 16-byte runtime state (4 states per 64-byte cache line).
  struct StateNode {
    uint32_t depth = 0u;
    uint32_t first_output = 0u;
    uint32_t next_output = 0u;
    uint32_t pattern_id = kInvalidPattern;
  };

  void EnsureCompiled() const {
    assert(is_compiled_ && "Call Compile() before searching");
    if (!is_compiled_) {
      throw std::logic_error(
          "AhoCorasick: Compile() must be called before searching");
    }
  }

  uint32_t Step(uint32_t state, char ch) const noexcept {
    const uint16_t col = char_to_col_[static_cast<uint8_t>(ch)];
    return transitions_[static_cast<std::size_t>(state) * alphabet_size_ + col];
  }

  std::vector<RawNode> raw_trie_;
  std::vector<StateNode> nodes_;
  std::vector<uint32_t> transitions_;
  std::array<uint16_t, 256u> char_to_col_{};
  uint32_t alphabet_size_ = 1u;
  uint32_t num_patterns_ = 0u;
  bool is_compiled_ = false;
};
