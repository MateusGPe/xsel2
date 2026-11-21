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
};

/**
 * Convenience base class for JSON-transforming steps that operate directly
 * on the target dictionary/document.
 */
class JsonTransformStep : public Step {
public:
  void apply(Node, std::vector<std::string> &, jsoncons::json &dict) final {
    dict = transform(dict);
  }

protected:
  [[nodiscard]] virtual jsoncons::json
  transform(const jsoncons::json &input) const = 0;
};

} // namespace scraper
