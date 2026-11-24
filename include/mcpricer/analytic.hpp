#pragma once

#include <optional>

#include "mcpricer/market.hpp"

namespace mcpricer {

[[nodiscard]] double normal_cdf(double x) noexcept;

/// Black-Scholes price. Uses the right, strike and maturity of `option`, and ignores its style,
/// so that it can also serve as a reference for other products.
[[nodiscard]] double black_scholes_price(const MarketParams& market, const OptionSpec& option);

struct Greeks {
  double delta;  ///< dV/dS
  double gamma;  ///< d2V/dS2
  double vega;   ///< dV/dsigma, per unit of volatility (not per percentage point)
  double theta;  ///< dV/dt in calendar time, per year
  double rho;    ///< dV/dr, per unit of rate
};

/// Throws std::invalid_argument when volatility * sqrt(maturity) is zero: the price then has a
/// kink at the money, so the Greeks are not defined everywhere.
[[nodiscard]] Greeks black_scholes_greeks(const MarketParams& market, const OptionSpec& option);

/// Closed form of a geometric-average Asian option monitored at i * T / N for i = 1..N.
[[nodiscard]] double geometric_asian_price(const MarketParams& market, const OptionSpec& option);

/// Exact price if this option has one; std::nullopt for arithmetic Asians.
[[nodiscard]] std::optional<double> analytic_price(const MarketParams& market,
                                                   const OptionSpec& option);

}  // namespace mcpricer
