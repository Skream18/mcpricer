// Statistical tests of the engine against closed forms. All seeds were fixed in advance and are
// never tuned to make a test pass; fixed seeds make every run deterministic.
#include "mcpricer/monte_carlo.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "mcpricer/analytic.hpp"

namespace mcpricer {
namespace {

const MarketParams kMarket{.spot = 100, .rate = 0.05, .dividend = 0.0, .volatility = 0.2};

PricingResult price(const MarketParams& market, const OptionSpec& option,
                    const SimulationConfig& config = {.paths = 200'000}) {
  return price_monte_carlo(market, option, config, *make_serial_backend());
}

/// Three standard errors, plus rounding slack for cases where the standard error is zero.
double tolerance(const PricingResult& result, double scale) {
  return 3.0 * result.std_error + 1e-10 * scale;
}

/// Least-squares slope of log(y) against log(x).
double log_log_slope(const std::vector<double>& x, const std::vector<double>& y) {
  const auto n = static_cast<double>(x.size());
  double sx = 0.0;
  double sy = 0.0;
  double sxx = 0.0;
  double sxy = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    const double lx = std::log(x[i]);
    const double ly = std::log(y[i]);
    sx += lx;
    sy += ly;
    sxx += lx * lx;
    sxy += lx * ly;
  }
  return (n * sxy - sx * sy) / (n * sxx - sx * sx);
}

struct Case {
  MarketParams market;
  double strike;
  double maturity;
};

void PrintTo(const Case& c, std::ostream* out) {
  *out << "S0=" << c.market.spot << " r=" << c.market.rate << " q=" << c.market.dividend
       << " sigma=" << c.market.volatility << " K=" << c.strike << " T=" << c.maturity;
}

class ClosedForm : public testing::TestWithParam<Case> {};

TEST_P(ClosedForm, EuropeanWithinThreeStandardErrors) {
  const auto& [market, strike, maturity] = GetParam();
  for (const OptionRight right : {OptionRight::Call, OptionRight::Put}) {
    const OptionSpec option{.right = right, .strike = strike, .maturity = maturity};
    const PricingResult result = price(market, option);
    EXPECT_NEAR(result.price, black_scholes_price(market, option), tolerance(result, market.spot));
  }
}

TEST_P(ClosedForm, GeometricAsianWithinThreeStandardErrors) {
  const auto& [market, strike, maturity] = GetParam();
  const OptionSpec option{
      .style = OptionStyle::AsianGeometric, .strike = strike, .maturity = maturity, .steps = 12};
  const PricingResult result = price(market, option);
  EXPECT_NEAR(result.price, geometric_asian_price(market, option), tolerance(result, market.spot));
}

INSTANTIATE_TEST_SUITE_P(
    ParameterSets, ClosedForm,
    testing::Values(Case{{100, 0.05, 0.0, 0.2}, 100, 1.0},     // at the money
                    Case{{100, 0.05, 0.0, 0.2}, 80, 1.0},      // in the money
                    Case{{100, 0.05, 0.0, 0.2}, 125, 1.0},     // out of the money
                    Case{{100, 0.03, 0.06, 0.35}, 110, 2.0},   // dividend above the rate
                    Case{{50, -0.01, 0.0, 0.1}, 50, 0.25},     // negative rate, short
                    Case{{100, 0.02, 0.01, 0.6}, 100, 5.0}));  // volatile, long

TEST(MonteCarlo, PutCallParity) {
  const OptionSpec call{.right = OptionRight::Call, .strike = 105};
  const OptionSpec put{.right = OptionRight::Put, .strike = 105};
  const PricingResult c = price(kMarket, call);
  const PricingResult p = price(kMarket, put);
  const double expected = kMarket.spot - 105 * std::exp(-kMarket.rate);
  // Equal seeds mean equal paths, so C - P is the sample mean of discounted S_T minus a
  // constant, and its standard error is that of discounted S_T, known in closed form.
  const double stdev_st = kMarket.spot * std::sqrt(std::exp(0.2 * 0.2) - 1.0);
  EXPECT_NEAR(c.price - p.price, expected, 3.0 * stdev_st / std::sqrt(200'000.0));
}

TEST(MonteCarlo, ArithmeticAsianRespectsBounds) {
  OptionSpec option{.style = OptionStyle::AsianArithmetic, .steps = 12};
  const PricingResult arithmetic = price(kMarket, option);
  option.style = OptionStyle::AsianGeometric;
  // The arithmetic mean is never below the geometric one, path by path.
  EXPECT_GT(arithmetic.price, price(kMarket, option).price);
  // Averaging reduces volatility, so with a positive rate an Asian call is below a European.
  EXPECT_LT(arithmetic.price, black_scholes_price(kMarket, option));
}

TEST(MonteCarlo, SingleStepArithmeticAsianEqualsEuropean) {
  const OptionSpec asian{.style = OptionStyle::AsianArithmetic, .steps = 1};
  EXPECT_NEAR(price(kMarket, asian).price, price(kMarket, OptionSpec{}).price, 1e-10);
}

TEST(MonteCarlo, EuropeanIgnoresSteps) {
  EXPECT_EQ(price(kMarket, OptionSpec{.steps = 1}).price,
            price(kMarket, OptionSpec{.steps = 252}).price);
}

TEST(Convergence, StandardErrorScalesAsInverseSquareRoot) {
  std::vector<double> paths;
  std::vector<double> errors;
  for (std::uint64_t n = 1'000; n <= 1'024'000; n *= 4) {
    paths.push_back(static_cast<double>(n));
    errors.push_back(price(kMarket, OptionSpec{}, {.paths = n}).std_error);
  }
  EXPECT_NEAR(log_log_slope(paths, errors), -0.5, 0.05);
}

// The reported standard error shrinks almost by construction. This checks the real error, which
// requires many independent runs per point.
TEST(Convergence, ActualErrorScalesAsInverseSquareRoot) {
  const double exact = black_scholes_price(kMarket, OptionSpec{});
  std::vector<double> paths;
  std::vector<double> errors;
  std::uint64_t seed = 1;
  for (std::uint64_t n = 500; n <= 32'000; n *= 4) {
    double squared = 0.0;
    constexpr int kRuns = 100;
    for (int run = 0; run < kRuns; ++run) {
      const double error = price(kMarket, OptionSpec{}, {.paths = n, .seed = seed++}).price - exact;
      squared += error * error;
    }
    paths.push_back(static_cast<double>(n));
    errors.push_back(std::sqrt(squared / kRuns));
  }
  EXPECT_NEAR(log_log_slope(paths, errors), -0.5, 0.1);
}

// If the standard error were wrong, for instance by using pairs as independent samples, the
// interval would cover the true price too often or too rarely.
TEST(Calibration, ConfidenceIntervalCoversTruePrice) {
  const double exact = black_scholes_price(kMarket, OptionSpec{});
  constexpr int kRuns = 400;
  int covered = 0;
  for (int run = 0; run < kRuns; ++run) {
    const PricingResult result = price(
        kMarket, OptionSpec{}, {.paths = 5'000, .seed = 1'000 + static_cast<std::uint64_t>(run)});
    if (result.ci_low <= exact && exact <= result.ci_high) ++covered;
  }
  EXPECT_GE(covered, 368);  // 92%
  EXPECT_LE(covered, 392);  // 98%
}

TEST(Reproducibility, SameSeedSameResultDifferentSeedDifferentResult) {
  const PricingResult a = price(kMarket, OptionSpec{}, {.paths = 10'000, .seed = 1});
  const PricingResult b = price(kMarket, OptionSpec{}, {.paths = 10'000, .seed = 1});
  const PricingResult c = price(kMarket, OptionSpec{}, {.paths = 10'000, .seed = 2});
  EXPECT_EQ(a.price, b.price);
  EXPECT_EQ(a.std_error, b.std_error);
  EXPECT_NE(a.price, c.price);
  EXPECT_NEAR(a.price, c.price, 3.0 * std::hypot(a.std_error, c.std_error));
}

TEST(EdgeCases, NoRandomnessGivesExactPrice) {
  const MarketParams no_vol{100, 0.05, 0.01, 0.0};
  for (const OptionStyle style : {OptionStyle::European, OptionStyle::AsianGeometric}) {
    const OptionSpec option{.style = style, .strike = 95, .steps = 12};
    const PricingResult result = price(no_vol, option, {.paths = 1'000});
    EXPECT_EQ(result.std_error, 0.0);
    EXPECT_NEAR(result.price, *analytic_price(no_vol, option), 1e-10);

    const OptionSpec expired{.style = style, .strike = 95, .maturity = 0.0, .steps = 12};
    const PricingResult at_expiry = price(kMarket, expired, {.paths = 1'000});
    EXPECT_EQ(at_expiry.price, 5.0);
    EXPECT_EQ(at_expiry.std_error, 0.0);
  }
}

TEST(EdgeCases, NearlyNoRandomness) {
  MarketParams tiny_vol = kMarket;
  tiny_vol.volatility = 1e-8;
  const OptionSpec option{.strike = 95};
  PricingResult result = price(tiny_vol, option, {.paths = 10'000});
  EXPECT_NEAR(result.price, black_scholes_price(tiny_vol, option), tolerance(result, 100));

  const OptionSpec about_to_expire{.strike = 100, .maturity = 1e-8};
  result = price(kMarket, about_to_expire, {.paths = 10'000});
  EXPECT_NEAR(result.price, black_scholes_price(kMarket, about_to_expire), tolerance(result, 100));
}

TEST(EdgeCases, DeepInAndOutOfTheMoney) {
  const OptionSpec deep_itm{.strike = 1};
  const PricingResult itm = price(kMarket, deep_itm);
  EXPECT_NEAR(itm.price, black_scholes_price(kMarket, deep_itm), tolerance(itm, 100));

  // No path gets there. The estimate of zero is right to within the absolute price, but a
  // standard error of zero is not a guarantee: plain Monte Carlo cannot see such rare events.
  const PricingResult otm = price(kMarket, OptionSpec{.strike = 10'000});
  EXPECT_EQ(otm.price, 0.0);
}

TEST(EdgeCases, InvalidInputsAreRejected) {
  EXPECT_THROW((void)price(kMarket, OptionSpec{}, {.paths = 0}), std::invalid_argument);
  EXPECT_THROW((void)price(kMarket, OptionSpec{}, {.paths = 1}), std::invalid_argument);
  EXPECT_THROW((void)price(kMarket, OptionSpec{}, {.paths = kMaxPaths + 1}), std::invalid_argument);
  EXPECT_THROW((void)price({.spot = -1}, OptionSpec{}), std::invalid_argument);
  EXPECT_THROW((void)price(kMarket, OptionSpec{.strike = 0}), std::invalid_argument);
}

TEST(EdgeCases, OverflowIsReportedNotReturned) {
  const MarketParams extreme{.spot = 100, .rate = 2'000, .volatility = 0.2};
  EXPECT_THROW((void)price(extreme, OptionSpec{}, {.paths = 100}), std::overflow_error);
}

}  // namespace
}  // namespace mcpricer
