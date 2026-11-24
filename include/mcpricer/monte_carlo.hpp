#pragma once

#include <chrono>
#include <cstdint>

#include "mcpricer/execution_backend.hpp"
#include "mcpricer/market.hpp"

namespace mcpricer {

struct SimulationConfig {
  std::uint64_t paths = 1'000'000;
  std::uint64_t seed = 42;
  bool antithetic = false;
  bool control_variate = false;
};

/// Bounds the memory a backend may use for partial results.
inline constexpr std::uint64_t kMaxPaths = 1'000'000'000;

struct PricingResult {
  double price = 0.0;
  double std_error = 0.0;
  double ci_low = 0.0;  ///< 95% confidence interval.
  double ci_high = 0.0;
  std::uint64_t paths = 0;    ///< Simulated paths.
  std::uint64_t samples = 0;  ///< Independent observations: paths, or pairs if antithetic.
  double control_beta = 0.0;
  std::chrono::duration<double> wall_time{};
};

/// Throws std::invalid_argument naming the offending field and its value.
void validate(const SimulationConfig& config);

/// Prices an option by Monte Carlo. Throws std::invalid_argument for invalid inputs and
/// std::overflow_error if inputs are so extreme that the estimate is not finite.
[[nodiscard]] PricingResult price_monte_carlo(const MarketParams& market, const OptionSpec& option,
                                              const SimulationConfig& config,
                                              const IExecutionBackend& backend);

}  // namespace mcpricer
