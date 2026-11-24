#include "mcpricer/monte_carlo.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <variant>

namespace mcpricer {
namespace {

constexpr double kNormalQuantile975 = 1.959963984540054;

SimulationTask make_task(const MarketParams& market, const OptionSpec& option,
                         const SimulationConfig& config) {
  SimulationTask task{.payoff = make_payoff(market, option)};
  const bool path_dependent =
      std::visit([](const auto& payoff) { return payoff.kPathDependent; }, task.payoff);
  task.steps = path_dependent ? option.steps : 1;
  const double dt = option.maturity / static_cast<double>(task.steps);
  task.step_drift =
      (market.rate - market.dividend - 0.5 * market.volatility * market.volatility) * dt;
  task.step_diffusion = market.volatility * std::sqrt(dt);
  task.discount = std::exp(-market.rate * option.maturity);
  task.seed = config.seed;
  task.samples = config.paths;
  return task;
}

}  // namespace

void validate(const SimulationConfig& config) {
  if (config.paths < 2 || config.paths > kMaxPaths) {
    throw std::invalid_argument("paths must be in [2, " + std::to_string(kMaxPaths) + "], got " +
                                std::to_string(config.paths));
  }
}

PricingResult price_monte_carlo(const MarketParams& market, const OptionSpec& option,
                                const SimulationConfig& config, const IExecutionBackend& backend) {
  validate(market);
  validate(option);
  validate(config);

  const auto start = std::chrono::steady_clock::now();
  const SimulationTask task = make_task(market, option, config);
  const BivariateAccumulator samples = backend.run(task);
  const Estimate estimate = estimate_mean(samples);
  const auto stop = std::chrono::steady_clock::now();

  if (!std::isfinite(estimate.mean) || !std::isfinite(estimate.std_error)) {
    throw std::overflow_error("the estimate is not finite; the inputs are too extreme");
  }
  const double half_width = kNormalQuantile975 * estimate.std_error;
  return {.price = estimate.mean,
          .std_error = estimate.std_error,
          .ci_low = estimate.mean - half_width,
          .ci_high = estimate.mean + half_width,
          .paths = config.paths,
          .samples = samples.count(),
          .wall_time = stop - start};
}

}  // namespace mcpricer
