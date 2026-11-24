#pragma once

#include <cstddef>

namespace mcpricer {

/// Inputs of geometric Brownian motion under the risk-neutral measure. Rates are annual and
/// continuously compounded; volatility is annual.
struct MarketParams {
  double spot = 100.0;
  double rate = 0.0;
  double dividend = 0.0;
  double volatility = 0.2;
};

enum class OptionRight { Call, Put };

enum class OptionStyle { European, AsianArithmetic, AsianGeometric };

struct OptionSpec {
  OptionStyle style = OptionStyle::European;
  OptionRight right = OptionRight::Call;
  double strike = 100.0;
  double maturity = 1.0;  ///< In years.
  /// Number of equally spaced monitoring dates of an Asian option. Today's spot is not one of
  /// them. Ignored by European options.
  std::size_t steps = 1;
};

inline constexpr std::size_t kMaxSteps = 1'000'000;

/// Both overloads throw std::invalid_argument naming the offending field and its value.
void validate(const MarketParams& market);
void validate(const OptionSpec& option);

}  // namespace mcpricer
