#include "mcpricer/payoffs.hpp"

#include <stdexcept>

namespace mcpricer {

AnyPayoff make_payoff(const MarketParams& market, const OptionSpec& option) {
  const VanillaTerms terms{.right = option.right, .spot = market.spot, .strike = option.strike};
  switch (option.style) {
    case OptionStyle::European:
      return EuropeanPayoff{terms};
    case OptionStyle::AsianArithmetic:
      return AsianArithmeticPayoff{terms};
    case OptionStyle::AsianGeometric:
      return AsianGeometricPayoff{terms};
  }
  throw std::invalid_argument("unknown option style");
}

}  // namespace mcpricer
