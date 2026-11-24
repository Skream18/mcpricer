#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "mcpricer/execution_backend.hpp"
#include "mcpricer/market.hpp"
#include "mcpricer/monte_carlo.hpp"

namespace mcpricer::cli {

enum class BackendKind { Serial, Threaded, Cuda };

struct Options {
  MarketParams market{.spot = 100.0, .rate = 0.05, .dividend = 0.0, .volatility = 0.2};
  OptionSpec option{.strike = 100.0, .maturity = 1.0, .steps = 252};
  SimulationConfig simulation;
  std::string type_name = "euro-call";
  BackendKind backend = BackendKind::Threaded;
  unsigned threads = 0;  ///< Zero means one per hardware thread.
  bool show_help = false;
  bool show_version = false;
};

/// Parses the arguments that follow the program name. Throws std::invalid_argument for unknown
/// flags and malformed values. Ranges are checked later, by the library.
[[nodiscard]] Options parse(std::span<const std::string_view> arguments);

[[nodiscard]] std::unique_ptr<IExecutionBackend> make_backend(const Options& options);

[[nodiscard]] std::string_view usage() noexcept;

}  // namespace mcpricer::cli
