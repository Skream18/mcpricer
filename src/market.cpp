#include "mcpricer/market.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace mcpricer {
namespace {

template <typename T>
void require(bool condition, std::string_view field, std::string_view rule, T value) {
  if (condition) return;
  std::ostringstream message;
  message << field << ' ' << rule << ", got " << value;
  throw std::invalid_argument(message.str());
}

}  // namespace

// Every comparison below is false for NaN, and isfinite also rejects infinities.
void validate(const MarketParams& market) {
  require(std::isfinite(market.spot) && market.spot > 0.0, "spot", "must be finite and positive",
          market.spot);
  require(std::isfinite(market.rate), "rate", "must be finite", market.rate);
  require(std::isfinite(market.dividend), "dividend", "must be finite", market.dividend);
  require(std::isfinite(market.volatility) && market.volatility >= 0.0, "volatility",
          "must be finite and non-negative", market.volatility);
}

void validate(const OptionSpec& option) {
  require(std::isfinite(option.strike) && option.strike > 0.0, "strike",
          "must be finite and positive", option.strike);
  require(std::isfinite(option.maturity) && option.maturity >= 0.0, "maturity",
          "must be finite and non-negative", option.maturity);
  require(option.steps >= 1 && option.steps <= kMaxSteps, "steps", "must be in [1, 1000000]",
          option.steps);
}

}  // namespace mcpricer
