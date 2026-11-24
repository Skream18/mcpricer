#pragma once

#include <memory>
#include <string_view>

#include "mcpricer/path_kernel.hpp"
#include "mcpricer/statistics.hpp"

namespace mcpricer {

/// Strategy for running a simulation. Virtual dispatch is harmless here because it happens once
/// per pricing. A backend returns statistics rather than a price, so that estimators and
/// confidence intervals exist in one place for all backends.
class IExecutionBackend {
 public:
  virtual ~IExecutionBackend() = default;

  [[nodiscard]] virtual BivariateAccumulator run(const SimulationTask& task) const = 0;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<IExecutionBackend> make_serial_backend();

}  // namespace mcpricer
