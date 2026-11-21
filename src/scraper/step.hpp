#pragma once

#include <absl/container/flat_hash_map.h>
#include <jsoncons/json.hpp>
#include <lxml2-query.h>
#include <string>
#include <vector>

namespace scraper {

/**
 * Per-node execution context shared across steps inside an Extractor pass
 * (named value slots for save/restore and short-circuit control flags).
 */
struct ExecutionContext {
  absl::flat_hash_map<std::string, std::vector<std::string>> slots;
  bool skip_item = false;
  bool abort_scraper = false;
};

/**
 * Base interface for all extraction, transformation, and JSON pipeline steps
 * executed by an Extractor.
 */
class Step {
public:
  virtual ~Step() = default;

  virtual void apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &dict) = 0;

  virtual void apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &dict, ExecutionContext &) {
    apply(node, values, dict);
  }

  [[nodiscard]] virtual bool is_postponable() const noexcept { return false; }

  [[nodiscard]] virtual jsoncons::json
  execute_postponed(const jsoncons::json &dest) const {
    return dest;
  }
};

/**
 * Convenience base class for JSON-transforming steps that support postponed
 * execution on the accumulated destination document.
 */
class PostponableJsonStep : public Step {
public:
  explicit PostponableJsonStep(bool postponable = true) noexcept
      : postponable_(postponable) {}

  void apply(Node, std::vector<std::string> &, jsoncons::json &dict) final {
    if (!postponable_) {
      dict = transform(dict);
    }
  }

  [[nodiscard]] bool is_postponable() const noexcept final {
    return postponable_;
  }

  [[nodiscard]] jsoncons::json
  execute_postponed(const jsoncons::json &dest) const final {
    return transform(dest);
  }

protected:
  [[nodiscard]] virtual jsoncons::json
  transform(const jsoncons::json &input) const = 0;

  bool postponable_;
};

} // namespace scraper
