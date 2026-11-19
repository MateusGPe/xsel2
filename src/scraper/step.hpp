#pragma once

#include <jsoncons/json.hpp>
#include <lxml2-query.h>
#include <string>
#include <vector>

namespace scraper {

/**
 * Base interface for all extraction, transformation, and JSON pipeline steps
 * executed by an Extractor.
 */
class Step {
public:
  virtual ~Step() = default;

  virtual void apply(Node node, std::vector<std::string> &values,
                     jsoncons::json &dict) = 0;

  [[nodiscard]] virtual bool is_postponable() const noexcept { return false; }

  [[nodiscard]] virtual jsoncons::json
  execute_postponed(const jsoncons::json &dest) const {
    return dest;
  }
};

} // namespace scraper
