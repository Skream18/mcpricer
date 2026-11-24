// Placeholder for the GPU backend; docs/CUDA_ROADMAP.md describes the planned implementation.
#include <stdexcept>

#include "mcpricer/execution_backend.hpp"

namespace mcpricer {

#ifdef MCPRICER_ENABLE_CUDA

namespace {

class CudaBackend final : public IExecutionBackend {
 public:
  BivariateAccumulator run(const SimulationTask&) const override {
    throw std::runtime_error("the CUDA backend is not implemented yet");
  }

  std::string_view name() const noexcept override { return "cuda"; }
};

}  // namespace

std::unique_ptr<IExecutionBackend> make_cuda_backend() { return std::make_unique<CudaBackend>(); }

#else

std::unique_ptr<IExecutionBackend> make_cuda_backend() {
  throw std::runtime_error("this build has no CUDA support (configure with -DENABLE_CUDA=ON)");
}

#endif

}  // namespace mcpricer
