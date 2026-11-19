#pragma once

#include "scraper/dsl/value.hpp"
#include "scraper/scraper.hpp"
#include <string_view>

namespace scraper::dsl {

/// Dispatches a DSL method or alias by name onto `builder`.
/// Throws `std::runtime_error` if `name` is unknown or arguments are invalid.
void apply_dsl_method(ScraperBuilder &builder, std::string_view name,
                      const DslArgList &args);

} // namespace scraper::dsl
