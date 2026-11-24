#include "mcpricer/analytic.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace mcpricer {
namespace {

double normal_pdf(double x) noexcept {
  return std::exp(-0.5 * x * x) * std::numbers::inv_sqrtpi / std::numbers::sqrt2;
}

/// Undiscounted E[max(X - K, 0)], or the put equivalent, for lognormal X with mean `forward`
/// and standard deviation of ln X equal to `stdev`. Black-Scholes and the geometric Asian
/// differ only in these two inputs.
double lognormal_expected_payoff(OptionRight right, double forward, double strike, double stdev) {
  if (stdev == 0.0) {  // X is deterministic, and d1 would divide by zero.
    const double intrinsic = right == OptionRight::Call ? forward - strike : strike - forward;
    return std::max(intrinsic, 0.0);
  }
  const double d1 = std::log(forward / strike) / stdev + 0.5 * stdev;
  const double d2 = d1 - stdev;
  // The put has its own formula: deriving it from the call by parity would subtract two large
  // numbers and lose all accuracy far out of the money.
  const double value = right == OptionRight::Call
                           ? forward * normal_cdf(d1) - strike * normal_cdf(d2)
                           : strike * normal_cdf(-d2) - forward * normal_cdf(-d1);
  return std::max(value, 0.0);  // Rounding can leave a tiny negative value.
}

}  // namespace

// erfc keeps relative accuracy in the left tail, where 0.5 * (1 + erf(x)) would cancel to zero.
double normal_cdf(double x) noexcept { return 0.5 * std::erfc(-x / std::numbers::sqrt2); }

double black_scholes_price(const MarketParams& market, const OptionSpec& option) {
  validate(market);
  validate(option);
  const double forward = market.spot * std::exp((market.rate - market.dividend) * option.maturity);
  const double stdev = market.volatility * std::sqrt(option.maturity);
  return std::exp(-market.rate * option.maturity) *
         lognormal_expected_payoff(option.right, forward, option.strike, stdev);
}

Greeks black_scholes_greeks(const MarketParams& market, const OptionSpec& option) {
  validate(market);
  validate(option);
  const double sqrt_t = std::sqrt(option.maturity);
  const double stdev = market.volatility * sqrt_t;
  if (stdev == 0.0) {
    throw std::invalid_argument("Greeks require volatility * sqrt(maturity) > 0");
  }
  const double carry = market.rate - market.dividend;
  const double d1 =
      (std::log(market.spot / option.strike) + carry * option.maturity) / stdev + 0.5 * stdev;
  const double d2 = d1 - stdev;
  const double spot_pv = market.spot * std::exp(-market.dividend * option.maturity);
  const double strike_pv = option.strike * std::exp(-market.rate * option.maturity);
  // Call and put formulas differ only by the sign applied to d1, d2 and the carry terms.
  const double sign = option.right == OptionRight::Call ? 1.0 : -1.0;
  const double n1 = normal_cdf(sign * d1);
  const double n2 = normal_cdf(sign * d2);

  return Greeks{
      .delta = sign * spot_pv * n1 / market.spot,
      .gamma = spot_pv * normal_pdf(d1) / (market.spot * market.spot * stdev),
      .vega = spot_pv * normal_pdf(d1) * sqrt_t,
      .theta = -spot_pv * normal_pdf(d1) * market.volatility / (2.0 * sqrt_t) -
               sign * market.rate * strike_pv * n2 + sign * market.dividend * spot_pv * n1,
      .rho = sign * option.maturity * strike_pv * n2,
  };
}

double geometric_asian_price(const MarketParams& market, const OptionSpec& option) {
  validate(market);
  validate(option);
  // ln G is the average of jointly normal variables, hence normal. Its moments follow from
  // sum(i) = N(N+1)/2 and sum_ij min(i, j) = N(N+1)(2N+1)/6.
  const auto n = static_cast<double>(option.steps);
  const double sigma2 = market.volatility * market.volatility;
  const double variance = sigma2 * option.maturity * (n + 1.0) * (2.0 * n + 1.0) / (6.0 * n * n);
  const double log_drift =
      (market.rate - market.dividend - 0.5 * sigma2) * option.maturity * (n + 1.0) / (2.0 * n);
  const double forward = market.spot * std::exp(log_drift + 0.5 * variance);
  return std::exp(-market.rate * option.maturity) *
         lognormal_expected_payoff(option.right, forward, option.strike, std::sqrt(variance));
}

std::optional<double> analytic_price(const MarketParams& market, const OptionSpec& option) {
  switch (option.style) {
    case OptionStyle::European:
      return black_scholes_price(market, option);
    case OptionStyle::AsianGeometric:
      return geometric_asian_price(market, option);
    case OptionStyle::AsianArithmetic:
      return std::nullopt;
  }
  throw std::invalid_argument("unknown option style");
}

}  // namespace mcpricer
