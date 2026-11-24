#include "mcpricer/analytic.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace mcpricer {
namespace {

OptionSpec european(OptionRight right, double strike, double maturity) {
  return {.style = OptionStyle::European, .right = right, .strike = strike, .maturity = maturity};
}

struct Case {
  MarketParams market;
  double strike;
  double maturity;
};

const std::vector<Case> kCases{
    {{.spot = 100, .rate = 0.05, .dividend = 0.0, .volatility = 0.2}, 100, 1.0},
    {{.spot = 100, .rate = 0.03, .dividend = 0.06, .volatility = 0.35}, 120, 2.0},
    {{.spot = 80, .rate = -0.01, .dividend = 0.01, .volatility = 0.15}, 70, 0.25},
    {{.spot = 100, .rate = 0.02, .dividend = 0.0, .volatility = 0.6}, 60, 5.0},
};

TEST(NormalCdf, KnownValuesAndLeftTail) {
  EXPECT_DOUBLE_EQ(normal_cdf(0.0), 0.5);
  EXPECT_NEAR(normal_cdf(1.959963984540054), 0.975, 1e-15);
  // The naive 0.5 * (1 + erf(x)) returns exactly zero here.
  EXPECT_NEAR(normal_cdf(-10.0) / 7.619853024160527e-24, 1.0, 1e-12);
}

TEST(BlackScholes, MatchesPublishedValues) {
  const MarketParams market{.spot = 100, .rate = 0.05, .volatility = 0.2};
  EXPECT_NEAR(black_scholes_price(market, european(OptionRight::Call, 100, 1)), 10.450583572185565,
              1e-10);
  EXPECT_NEAR(black_scholes_price(market, european(OptionRight::Put, 100, 1)), 5.573526022256971,
              1e-10);
  // Hull, "Options, Futures, and Other Derivatives": 4.76 and 0.81.
  const MarketParams hull{.spot = 42, .rate = 0.1, .volatility = 0.2};
  EXPECT_NEAR(black_scholes_price(hull, european(OptionRight::Call, 40, 0.5)), 4.76, 0.005);
  EXPECT_NEAR(black_scholes_price(hull, european(OptionRight::Put, 40, 0.5)), 0.81, 0.005);
}

TEST(BlackScholes, PutCallParity) {
  for (const auto& [market, strike, maturity] : kCases) {
    const double call = black_scholes_price(market, european(OptionRight::Call, strike, maturity));
    const double put = black_scholes_price(market, european(OptionRight::Put, strike, maturity));
    const double expected = market.spot * std::exp(-market.dividend * maturity) -
                            strike * std::exp(-market.rate * maturity);
    EXPECT_NEAR(call - put, expected, 1e-12 * market.spot);
  }
}

TEST(BlackScholes, DegenerateInputsGiveDiscountedIntrinsic) {
  const MarketParams no_vol{.spot = 100, .rate = 0.05, .volatility = 0.0};
  EXPECT_NEAR(black_scholes_price(no_vol, european(OptionRight::Call, 90, 1)),
              100 - 90 * std::exp(-0.05), 1e-12);
  EXPECT_EQ(black_scholes_price(no_vol, european(OptionRight::Put, 90, 1)), 0.0);

  const MarketParams market{.spot = 100, .rate = 0.05, .volatility = 0.2};
  EXPECT_EQ(black_scholes_price(market, european(OptionRight::Call, 90, 0)), 10.0);
  EXPECT_EQ(black_scholes_price(market, european(OptionRight::Put, 90, 0)), 0.0);
}

TEST(BlackScholes, ExtremeStrikes) {
  const MarketParams market{.spot = 100, .rate = 0.05, .volatility = 0.2};
  const double deep_otm = black_scholes_price(market, european(OptionRight::Call, 1000, 1));
  EXPECT_GE(deep_otm, 0.0);
  EXPECT_LT(deep_otm, 1e-20);
  EXPECT_NEAR(black_scholes_price(market, european(OptionRight::Call, 1, 1)), 100 - std::exp(-0.05),
              1e-12);
}

TEST(BlackScholes, GreeksMatchFiniteDifferences) {
  for (const auto& [market, strike, maturity] : kCases) {
    for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
      const OptionSpec option = european(right, strike, maturity);
      const Greeks greeks = black_scholes_greeks(market, option);
      // Central difference of the price with respect to one field.
      const auto derivative = [&](auto member, double h) {
        MarketParams up = market;
        MarketParams down = market;
        up.*member += h;
        down.*member -= h;
        return (black_scholes_price(up, option) - black_scholes_price(down, option)) / (2 * h);
      };
      EXPECT_NEAR(greeks.delta, derivative(&MarketParams::spot, 1e-3), 1e-7);
      EXPECT_NEAR(greeks.vega, derivative(&MarketParams::volatility, 1e-5), 1e-5);
      EXPECT_NEAR(greeks.rho, derivative(&MarketParams::rate, 1e-5), 1e-5);

      MarketParams up = market;
      MarketParams down = market;
      const double h = 0.01;
      up.spot += h;
      down.spot -= h;
      const double gamma =
          (black_scholes_price(up, option) - 2 * black_scholes_price(market, option) +
           black_scholes_price(down, option)) /
          (h * h);
      EXPECT_NEAR(greeks.gamma, gamma, 1e-6);

      // Calendar time runs against time to maturity, hence the sign.
      const double dt = 1e-5;
      const double theta = -(black_scholes_price(market, european(right, strike, maturity + dt)) -
                             black_scholes_price(market, european(right, strike, maturity - dt))) /
                           (2 * dt);
      EXPECT_NEAR(greeks.theta, theta, 1e-5);
    }
  }
}

TEST(BlackScholes, GreeksRejectDegenerateInputs) {
  EXPECT_THROW((void)black_scholes_greeks({.volatility = 0.0}, OptionSpec{}),
               std::invalid_argument);
  EXPECT_THROW((void)black_scholes_greeks({}, OptionSpec{.maturity = 0.0}), std::invalid_argument);
}

OptionSpec geometric(OptionRight right, double strike, double maturity, std::size_t steps) {
  return {.style = OptionStyle::AsianGeometric,
          .right = right,
          .strike = strike,
          .maturity = maturity,
          .steps = steps};
}

TEST(GeometricAsian, SingleStepEqualsBlackScholes) {
  for (const auto& [market, strike, maturity] : kCases) {
    for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
      EXPECT_NEAR(geometric_asian_price(market, geometric(right, strike, maturity, 1)),
                  black_scholes_price(market, european(right, strike, maturity)), 1e-11);
    }
  }
}

// Independent derivation: sum the covariances of the log prices term by term instead of using
// the closed-form sums, then integrate the payoff against the normal density numerically.
TEST(GeometricAsian, MatchesNumericalIntegration) {
  const std::size_t steps = 12;
  for (const auto& [market, strike, maturity] : kCases) {
    const double dt = maturity / static_cast<double>(steps);
    double mean = std::log(market.spot);
    double variance = 0.0;
    for (std::size_t i = 1; i <= steps; ++i) {
      mean += (market.rate - market.dividend - 0.5 * market.volatility * market.volatility) * dt *
              static_cast<double>(i) / static_cast<double>(steps);
      for (std::size_t j = 1; j <= steps; ++j) {
        variance += market.volatility * market.volatility * dt *
                    static_cast<double>(std::min(i, j)) / static_cast<double>(steps * steps);
      }
    }
    const int intervals = 400'000;
    const double width = 24.0 / intervals;
    double call = 0.0;
    double put = 0.0;
    for (int k = 0; k <= intervals; ++k) {
      const double z = -12.0 + width * k;
      const double average = std::exp(mean + std::sqrt(variance) * z);
      const double weight =
          width * std::exp(-0.5 * z * z) * std::numbers::inv_sqrtpi / std::numbers::sqrt2;
      call += weight * std::max(average - strike, 0.0);
      put += weight * std::max(strike - average, 0.0);
    }
    const double discount = std::exp(-market.rate * maturity);
    EXPECT_NEAR(
        geometric_asian_price(market, geometric(OptionRight::Call, strike, maturity, steps)),
        discount * call, 1e-6);
    EXPECT_NEAR(geometric_asian_price(market, geometric(OptionRight::Put, strike, maturity, steps)),
                discount * put, 1e-6);
  }
}

TEST(AnalyticPrice, AvailableOnlyWhereAClosedFormExists) {
  EXPECT_TRUE(analytic_price({}, {.style = OptionStyle::European}).has_value());
  EXPECT_TRUE(analytic_price({}, {.style = OptionStyle::AsianGeometric}).has_value());
  EXPECT_FALSE(analytic_price({}, {.style = OptionStyle::AsianArithmetic}).has_value());
}

}  // namespace
}  // namespace mcpricer
