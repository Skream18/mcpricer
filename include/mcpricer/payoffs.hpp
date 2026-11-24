#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <variant>

#include "mcpricer/market.hpp"

namespace mcpricer {

/// What a payoff remembers while a path evolves. `log_return` is ln(S / S0): simulating it
/// instead of S turns each step into an addition and makes S = S0 exact when nothing moves.
struct PathState {
  double log_return = 0.0;
  double sum_growth = 0.0;  ///< Sum of S / S0 over the monitoring dates.
  double sum_log = 0.0;     ///< Sum of ln(S / S0) over the monitoring dates.
};

/// Undiscounted payoff of a path, and the control variate observed on the same path.
struct PathResult {
  double payoff = 0.0;
  double control = 0.0;
};

/// The common interface of all payoffs. It is a concept rather than a base class with virtual
/// functions so that the simulation loop can inline it: a virtual call per step would be slow
/// on a CPU, and virtual objects created on the host cannot be called on a GPU.
template <typename P>
concept Payoff = requires(const P payoff, PathState& state, std::size_t steps) {
  /// Whether intermediate prices matter. If not, one exact step to maturity is enough.
  { P::kPathDependent } -> std::convertible_to<bool>;
  { payoff.observe(state) } noexcept -> std::same_as<void>;
  { payoff.evaluate(state, steps) } noexcept -> std::same_as<PathResult>;
};

struct VanillaTerms {
  OptionRight right;
  double spot;
  double strike;

  [[nodiscard]] double intrinsic(double underlying) const noexcept {
    const double value = right == OptionRight::Call ? underlying - strike : strike - underlying;
    return std::max(value, 0.0);
  }
};

struct EuropeanPayoff {
  static constexpr bool kPathDependent = false;
  VanillaTerms terms;

  void observe(PathState&) const noexcept {}
  /// Control: the terminal price, whose discounted mean is known by no arbitrage.
  [[nodiscard]] PathResult evaluate(const PathState& state, std::size_t) const noexcept {
    const double terminal = terms.spot * std::exp(state.log_return);
    return {.payoff = terms.intrinsic(terminal), .control = terminal};
  }
};

struct AsianArithmeticPayoff {
  static constexpr bool kPathDependent = true;
  VanillaTerms terms;

  void observe(PathState& state) const noexcept {
    state.sum_growth += std::exp(state.log_return);
    state.sum_log += state.log_return;
  }
  /// Control: the geometric Asian on the same path. The two averages are nearly equal, so the
  /// payoffs are almost perfectly correlated, and the geometric one has a closed form.
  [[nodiscard]] PathResult evaluate(const PathState& state, std::size_t steps) const noexcept {
    const auto n = static_cast<double>(steps);
    const double arithmetic = terms.spot * (state.sum_growth / n);
    const double geometric = terms.spot * std::exp(state.sum_log / n);
    return {.payoff = terms.intrinsic(arithmetic), .control = terms.intrinsic(geometric)};
  }
};

struct AsianGeometricPayoff {
  static constexpr bool kPathDependent = true;
  VanillaTerms terms;

  void observe(PathState& state) const noexcept { state.sum_log += state.log_return; }
  /// No control: this option is the control of the arithmetic one.
  [[nodiscard]] PathResult evaluate(const PathState& state, std::size_t steps) const noexcept {
    const double geometric = terms.spot * std::exp(state.sum_log / static_cast<double>(steps));
    return {.payoff = terms.intrinsic(geometric), .control = 0.0};
  }
};

static_assert(Payoff<EuropeanPayoff>);
static_assert(Payoff<AsianArithmeticPayoff>);
static_assert(Payoff<AsianGeometricPayoff>);

using AnyPayoff = std::variant<EuropeanPayoff, AsianArithmeticPayoff, AsianGeometricPayoff>;

[[nodiscard]] AnyPayoff make_payoff(const MarketParams& market, const OptionSpec& option);

}  // namespace mcpricer
