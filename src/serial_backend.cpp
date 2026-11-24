#include "mcpricer/execution_backend.hpp"

namespace mcpricer {
namespace {

class SerialBackend final : public IExecutionBackend {
 public:
  BivariateAccumulator run(const SimulationTask& task) const override {
    BivariateAccumulator total;
    const std::uint64_t chunks = chunk_count(task.samples);
    for (std::uint64_t chunk = 0; chunk < chunks; ++chunk) {
      total.merge(simulate_chunk(task, chunk));
    }
    return total;
  }

  std::string_view name() const noexcept override { return "serial"; }
};

}  // namespace

std::unique_ptr<IExecutionBackend> make_serial_backend() {
  return std::make_unique<SerialBackend>();
}

}  // namespace mcpricer
