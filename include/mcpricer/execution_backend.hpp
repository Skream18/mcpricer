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

inline constexpr unsigned kMaxThreads = 1024;

/// Returns exactly the same statistics as the serial backend, for any number of threads.
/// Zero threads means one per hardware thread. Throws std::invalid_argument above kMaxThreads.
[[nodiscard]] std::unique_ptr<IExecutionBackend> make_threaded_backend(unsigned threads = 0);

/// Not implemented yet. Throws std::runtime_error here unless built with ENABLE_CUDA, and from
/// run() otherwise. Always declared, so that callers need no conditional compilation.
[[nodiscard]] std::unique_ptr<IExecutionBackend> make_cuda_backend();

}  // namespace mcpricer
