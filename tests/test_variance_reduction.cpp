#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <utility>

#include "mcpricer/analytic.hpp"
#include "mcpricer/monte_carlo.hpp"

namespace mcpricer {
namespace {

const MarketParams kMarket{.spot = 100, .rate = 0.05, .dividend = 0.02, .volatility = 0.2};
constexpr std::uint64_t kPaths = 100'000;

PricingResult price(const OptionSpec& option, bool antithetic, bool control,
                    std::uint64_t paths = kPaths, std::uint64_t seed = 42) {
  return price_monte_carlo(
      kMarket, option,
      {.paths = paths, .seed = seed, .antithetic = antithetic, .control_variate = control},
      *make_serial_backend());
}

OptionSpec make_option(OptionStyle style, OptionRight right) {
  return {.style = style, .right = right, .strike = 100, .maturity = 1.0, .steps = 12};
}

// Every comparison uses the same number of simulated paths. Antithetic runs draw half the normals,
// so they are not more expensive.
TEST(VarianceReduction, AntitheticLowersStandardError) {
  for (const OptionStyle style :
       {OptionStyle::European, OptionStyle::AsianArithmetic, OptionStyle::AsianGeometric}) {
    for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
      const OptionSpec option = make_option(style, right);
      EXPECT_LT(price(option, true, false).std_error, 0.9 * price(option, false, false).std_error);
    }
  }
}

// The theoretical ratio is sqrt(1 - rho^2), with rho = corr(payoff, S_T). The constants come from
// numerical integration over the normal density, independently of this code.
TEST(VarianceReduction, EuropeanControlMatchesTheory) {
  for (const auto& [right, expected] :
       {std::pair{OptionRight::Call, 0.4107}, {OptionRight::Put, 0.6199}}) {
    const OptionSpec option = make_option(OptionStyle::European, right);
    const double ratio =
        price(option, false, true).std_error / price(option, false, false).std_error;
    EXPECT_NEAR(ratio, expected, 0.02 * expected);
  }
}

TEST(VarianceReduction, GeometricControlCutsArithmeticErrorTenfold) {
  for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
    const OptionSpec asian = make_option(OptionStyle::AsianArithmetic, right);
    EXPECT_LT(price(asian, false, true).std_error, 0.1 * price(asian, false, false).std_error);
  }
}

TEST(VarianceReduction, EstimatesAgreeWithReference) {
  for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
    const OptionSpec european = make_option(OptionStyle::European, right);
    const double exact = black_scholes_price(kMarket, european);
    for (const auto& [antithetic, control] :
         {std::pair{true, false}, {false, true}, {true, true}}) {
      const PricingResult result = price(european, antithetic, control);
      EXPECT_NEAR(result.price, exact, 3.0 * result.std_error) << antithetic << control;
      EXPECT_LT(result.std_error, price(european, false, false).std_error);
    }
    // No closed form here: compare with an independent plain run on another seed.
    const OptionSpec asian = make_option(OptionStyle::AsianArithmetic, right);
    const PricingResult plain = price(asian, false, false, 400'000, 7);
    for (const auto& [antithetic, control] :
         {std::pair{true, false}, {false, true}, {true, true}}) {
      const PricingResult result = price(asian, antithetic, control);
      EXPECT_NEAR(result.price, plain.price, 3.0 * std::hypot(result.std_error, plain.std_error));
    }
  }
}

TEST(VarianceReduction, AntitheticCountsPairsAsSamples) {
  const OptionSpec option = make_option(OptionStyle::European, OptionRight::Call);
  EXPECT_EQ(price(option, false, false).samples, kPaths);
  EXPECT_EQ(price(option, true, false).samples, kPaths / 2);
  EXPECT_EQ(price(option, true, false).paths, kPaths);
}

// The reported errors must be honest, not merely smaller.
TEST(VarianceReduction, ConfidenceIntervalsStayCalibrated) {
  const OptionSpec option = make_option(OptionStyle::European, OptionRight::Call);
  const double exact = black_scholes_price(kMarket, option);
  for (const auto& [antithetic, control] : {std::pair{true, false}, {false, true}, {true, true}}) {
    int covered = 0;
    for (std::uint64_t seed = 5'000; seed < 5'400; ++seed) {
      const PricingResult result = price(option, antithetic, control, 4'000, seed);
      if (result.ci_low <= exact && exact <= result.ci_high) ++covered;
    }
    EXPECT_GE(covered, 368) << antithetic << control;  // 92% of 400
    EXPECT_LE(covered, 392) << antithetic << control;  // 98% of 400
  }
}

TEST(VarianceReduction, DegenerateControlIsHarmless) {
  const MarketParams no_vol{.spot = 100, .rate = 0.05, .volatility = 0.0};
  const OptionSpec option = make_option(OptionStyle::European, OptionRight::Call);
  const PricingResult result = price_monte_carlo(
      no_vol, option, {.paths = 1'000, .antithetic = true, .control_variate = true},
      *make_serial_backend());
  EXPECT_EQ(result.control_beta, 0.0);
  EXPECT_NEAR(result.price, black_scholes_price(no_vol, option), 1e-10);
}

TEST(VarianceReduction, InvalidCombinationsAreRejected) {
  const OptionSpec european = make_option(OptionStyle::European, OptionRight::Call);
  const OptionSpec geometric = make_option(OptionStyle::AsianGeometric, OptionRight::Call);
  EXPECT_THROW((void)price(geometric, false, true), std::invalid_argument);
  EXPECT_THROW((void)price(european, true, false, 1'001), std::invalid_argument);
  EXPECT_THROW((void)price(european, true, false, 2), std::invalid_argument);
  EXPECT_THROW((void)price(european, false, true, 3), std::invalid_argument);
  EXPECT_THROW((void)price(european, true, true, 6), std::invalid_argument);
  EXPECT_NO_THROW((void)price(european, true, true, 8));
}

}  // namespace
}  // namespace mcpricer
