#include "mcpricer/market.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace mcpricer {
namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

template <typename Params>
void expect_rejected(const std::string& field, const std::function<void(Params&)>& corrupt) {
  Params params;
  corrupt(params);
  try {
    validate(params);
    ADD_FAILURE() << "expected " << field << " to be rejected";
  } catch (const std::invalid_argument& error) {
    EXPECT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
  }
}

TEST(Validation, AcceptsDefaultsAndBoundaryValues) {
  EXPECT_NO_THROW(validate(MarketParams{}));
  EXPECT_NO_THROW(validate(OptionSpec{}));
  EXPECT_NO_THROW(validate(MarketParams{.rate = -0.01, .dividend = -0.01, .volatility = 0.0}));
  EXPECT_NO_THROW(validate(OptionSpec{.maturity = 0.0, .steps = kMaxSteps}));
}

TEST(Validation, RejectsInvalidMarket) {
  for (const double bad : {0.0, -1.0, kNan, kInf}) {
    expect_rejected<MarketParams>("spot", [bad](MarketParams& m) { m.spot = bad; });
  }
  for (const double bad : {-0.1, kNan, kInf}) {
    expect_rejected<MarketParams>("volatility", [bad](MarketParams& m) { m.volatility = bad; });
  }
  for (const double bad : {kNan, kInf, -kInf}) {
    expect_rejected<MarketParams>("rate", [bad](MarketParams& m) { m.rate = bad; });
    expect_rejected<MarketParams>("dividend", [bad](MarketParams& m) { m.dividend = bad; });
  }
}

TEST(Validation, RejectsInvalidOption) {
  for (const double bad : {0.0, -1.0, kNan, kInf}) {
    expect_rejected<OptionSpec>("strike", [bad](OptionSpec& o) { o.strike = bad; });
  }
  for (const double bad : {-1.0, kNan, kInf}) {
    expect_rejected<OptionSpec>("maturity", [bad](OptionSpec& o) { o.maturity = bad; });
  }
  expect_rejected<OptionSpec>("steps", [](OptionSpec& o) { o.steps = 0; });
  expect_rejected<OptionSpec>("steps", [](OptionSpec& o) { o.steps = kMaxSteps + 1; });
}

}  // namespace
}  // namespace mcpricer
