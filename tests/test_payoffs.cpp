#include "mcpricer/payoffs.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace mcpricer {
namespace {

constexpr VanillaTerms kCall{.right = OptionRight::Call, .spot = 100, .strike = 100};
constexpr VanillaTerms kPut{.right = OptionRight::Put, .spot = 100, .strike = 100};

/// Feeds a path through two monitoring dates, at prices 121 and 81.
template <Payoff P>
PathResult evaluate_two_dates(const P& payoff) {
  PathState state;
  for (const double price : {121.0, 81.0}) {
    state.log_return = std::log(price / 100.0);
    payoff.observe(state);
  }
  return payoff.evaluate(state, 2);
}

TEST(Payoffs, EuropeanUsesOnlyTheTerminalPrice) {
  EXPECT_DOUBLE_EQ(evaluate_two_dates(EuropeanPayoff{kCall}).payoff, 0.0);
  EXPECT_NEAR(evaluate_two_dates(EuropeanPayoff{kPut}).payoff, 19.0, 1e-12);
  EXPECT_NEAR(evaluate_two_dates(EuropeanPayoff{kPut}).control, 81.0, 1e-12);
}

TEST(Payoffs, ArithmeticAveragesPricesAndControlsWithGeometric) {
  // Arithmetic mean 101, geometric mean sqrt(121 * 81) = 99.
  EXPECT_NEAR(evaluate_two_dates(AsianArithmeticPayoff{kCall}).payoff, 1.0, 1e-12);
  EXPECT_NEAR(evaluate_two_dates(AsianArithmeticPayoff{kCall}).control, 0.0, 1e-12);
  EXPECT_NEAR(evaluate_two_dates(AsianArithmeticPayoff{kPut}).payoff, 0.0, 1e-12);
  EXPECT_NEAR(evaluate_two_dates(AsianArithmeticPayoff{kPut}).control, 1.0, 1e-12);
}

TEST(Payoffs, GeometricAveragesLogPrices) {
  EXPECT_NEAR(evaluate_two_dates(AsianGeometricPayoff{kCall}).payoff, 0.0, 1e-12);
  EXPECT_NEAR(evaluate_two_dates(AsianGeometricPayoff{kPut}).payoff, 1.0, 1e-12);
}

TEST(Payoffs, FactorySelectsByStyle) {
  EXPECT_TRUE(
      std::holds_alternative<EuropeanPayoff>(make_payoff({}, {.style = OptionStyle::European})));
  EXPECT_TRUE(std::holds_alternative<AsianArithmeticPayoff>(
      make_payoff({}, {.style = OptionStyle::AsianArithmetic})));
  EXPECT_TRUE(std::holds_alternative<AsianGeometricPayoff>(
      make_payoff({}, {.style = OptionStyle::AsianGeometric})));
}

}  // namespace
}  // namespace mcpricer
