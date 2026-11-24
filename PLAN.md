# PLAN: mcpricer, a Monte Carlo option pricer in C++20

## Context

A C++ code sample for a job application at an AI trading firm. Reviewers judge code quality, rigor and documentation, and the author must be able to defend every decision in an interview. So: a small core, testable claims, and one clear reason per choice. CPU-first, with a defined seam where a CUDA backend can be added later.

**Gate.** Approving this draft means: create the project repo and commit this text as `PLAN.md`, then stop. Code starts only on "proceed" (approving with the word "proceed" covers both). One commit per milestone, then a short summary and a pause for questions.

## 0. Environment

| Item | Found | Verdict |
|---|---|---|
| MSVC | 14.51 (VS Build Tools 2026, 18.11, Windows 11 SDK 26100) | **Primary compiler** |
| g++ | 15.2.0, MSYS2 UCRT64, posix threads | Secondary compiler, portability check |
| CMake / Ninja | 4.3.1 / 1.13.2, bundled with Build Tools, not on PATH | Usable via full path |
| clang-format | Bundled with the VS Code C++ extension, not on PATH | Usable via full path |
| clang | Not installed | Not needed |
| git / gh | 2.52 / logged in | OK |
| CPU | i7-14650HX, 16 cores (8 P + 8 E), 24 threads | Hybrid: affects scheduling and benchmarks |
| GPU | RTX 5050 Laptop, 8 GB, compute capability 12.0, driver 592.00 (supports up to CUDA 13.1) | CUDA-capable |
| nvcc | Not installed | Only needed at M9 |
| Python | `python` is 3.15, without matplotlib. The Python 3.14 install has matplotlib 3.10.8 | Use 3.14 for plots |

**Install now: nothing.** Presets use the `Visual Studio 18 2026` generator (finds MSVC without a developer prompt) and `MinGW Makefiles`. For convenience, in Git Bash:

```bash
export PATH="/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin:$PATH"
```

**For M9 only:** a CUDA 13.x toolkit. [NVIDIA's Windows guide](https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html) lists VS 2026 (MSVC 195x), but support varies by release, and [one user reports](https://forums.developer.nvidia.com/t/cuda-13-4-1-nvcc-13-4-59-rejects-complete-visual-studio-build-tools-2026-with-host-compiler-targets-unsupported-os/383253) CUDA 13.4.1 rejecting VS 2026 Build Tools. A toolkit newer than 13.1 may also need a driver update. Recheck then; the fallback is installing the VS 2022 toolset.

**Repository hazard.** `C:\Users\dudep` is itself a git repo with remote `Skream18/pawfect`, and this folder is inside it. Run `git init -b main` **in this folder**, and verify `git rev-parse --show-toplevel` before each commit. The home repo will not be modified. No remote or push unless asked.

## 1. Architecture

```
CLI / bench / tests
      |
MonteCarloEngine   validate -> run backend -> estimate -> PricingResult
      |
IExecutionBackend  virtual, called once per pricing
  |- SerialBackend
  |- ThreadedBackend
  `- CudaBackend (stub)
      |
simulate_chunk<Payoff>   shared CPU kernel, fully inlined
      |
Philox + Box-Muller, GBM step, payoff, accumulator
```

Two rules drive the design:

1. **Dynamic dispatch outside, static dispatch inside.** A virtual call per pricing costs nothing; one per path blocks inlining, and host-created virtual objects cannot be called on a GPU. Payoffs are therefore small value types satisfying a C++20 `Payoff` concept, held in a `std::variant` and visited once per chunk. The concept is the common interface.
2. **A backend returns mergeable statistics, not a price.** Control variates and confidence intervals are computed afterwards in one place, so they are identical for every backend, including a future GPU one.

## 2. Layout

```
CMakeLists.txt  CMakePresets.json  .clang-format  .gitignore  LICENSE  README.md  PLAN.md
include/mcpricer/
  market.hpp              MarketParams, OptionSpec, validate()
  payoffs.hpp             payoff structs, Payoff concept, variant
  analytic.hpp            Black-Scholes, geometric Asian
  philox.hpp              Philox4x32-10, uniform and normal conversion
  statistics.hpp          bivariate mergeable accumulator, estimator
  path_kernel.hpp         simulate_chunk
  execution_backend.hpp   IExecutionBackend, SimulationTask, factory
  monte_carlo.hpp         SimulationConfig, PricingResult, engine
src/        analytic, market, statistics, monte_carlo, serial/threaded/cuda backends
apps/       mcpricer.cpp, cli_parser.{hpp,cpp}
tests/      one file per module
bench/      bench_main.cpp
scripts/    plot.py
docs/       LEARNING.md, CUDA_ROADMAP.md
.github/workflows/ci.yml
```

## 3. Key types

```cpp
struct MarketParams { double spot, rate, dividend, volatility; };
struct OptionSpec   { OptionType type; double strike, maturity; std::size_t steps; };

struct SimulationConfig {
  std::uint64_t paths = 1'000'000, seed = 42;
  bool antithetic = false, control_variate = false;
};

struct PricingResult {
  double price, std_error, ci_low, ci_high;
  std::uint64_t paths, samples;          // samples = paths/2 when antithetic
  double control_beta;
  std::chrono::duration<double> wall_time;
};

class IExecutionBackend {
 public:
  virtual ~IExecutionBackend() = default;
  virtual BivariateAccumulator run(const SimulationTask&) const = 0;
  virtual std::string_view name() const noexcept = 0;
};
```

- `--type`: `euro-call`, `euro-put`, `asian-arith-call`, `asian-arith-put`, `asian-geo-call`. `asian-arith` (your example) is an alias for the call.
- The engine takes a `const IExecutionBackend&`. Factories return `unique_ptr`.
- Validation throws `std::invalid_argument` naming the field and value: all inputs finite, S0 > 0, K > 0, sigma >= 0, T >= 0, steps >= 1, paths >= 2 (antithetic: even and >= 4, because at least two samples are needed), checked maximums.
- With control variates, at least 4 samples are required.

## 4. Math

**Model.** dS = (r-q)S dt + sigma S dW, simulated exactly in log space:
`ln S(t+dt) = ln S(t) + (r - q - sigma^2/2) dt + sigma sqrt(dt) Z`.
There is no discretisation bias for GBM, so European options use one step regardless of `--steps`. Asians monitor at t_i = iT/N, i = 1..N; S0 is excluded.

**Estimator.** X = e^(-rT) payoff; price = sample mean; SE = s/sqrt(n) with the n-1 sample variance; 95% CI = mean +/- 1.959964 SE. Accumulation uses Welford's update and the Chan et al. pairwise merge, which are numerically stable and parallel.

**Black-Scholes.** With forward F = S0 e^((r-q)T):
`C = e^(-rT)[F N(d1) - K N(d2)]`, `d1 = [ln(F/K) + sigma^2 T/2]/(sigma sqrt(T))`, `d2 = d1 - sigma sqrt(T)`. N(x) uses `erfc` for tail accuracy. If sigma sqrt(T) = 0, return the discounted intrinsic on the forward. Delta, gamma, vega, theta and rho included, each tested by finite differences; undefined at zero sigma sqrt(T), where they throw.

**Geometric Asian.** ln G is normal with
`m = ln S0 + (r - q - sigma^2/2) T (N+1)/(2N)` and `v = sigma^2 T (N+1)(2N+1)/(6N^2)`.
`C = e^(-rT)[e^(m+v/2) N(d1) - K N(d2)]`, `d2 = (m - ln K)/sqrt(v)`, `d1 = d2 + sqrt(v)`. The put form is also needed, as the control for arithmetic puts.

**Antithetic.** One sample is (X(Z) + X(-Z))/2. The SE is computed over **pairs**, because the two halves are dependent. `--paths` counts simulated paths, so comparisons are at roughly equal cost. The payoffs are monotone in Z, so the covariance is non-positive.

**Control variate.** `X_cv = X - beta (Y - E[Y])`, `beta = Cov(X,Y)/Var(Y)`. One pass collects means and co-moments; beta is then estimated from the same sample. This adds O(1/n) bias against an O(1/sqrt(n)) SE, and needs no pilot run. Residual variance uses n-2. If Var(Y) = 0, beta = 0.
- European: Y = e^(-rT) S_T, E[Y] = S0 e^(-qT).
- Arithmetic Asian: Y = discounted geometric payoff of the same type, with the closed form.
- Geometric Asian: rejected. It would be its own control and turn the MC check into a tautology.
- With antithetic on, the control applies to pair averages.

## 5. RNG: Philox4x32-10, hand-written, with Box-Muller

| Option | Problem or benefit |
|---|---|
| mt19937_64, seed + thread id | 2.5 KB state per stream; independence not guaranteed; results depend on thread count |
| Jump-ahead generators | Sound, but stateful and still tied to the partition |
| **Philox** | Stateless function of (key, counter); tiny; cuRAND provides the same family |

`std::philox_engine` is C++26, so it is written here (about 50 lines) and checked against the published Random123 known-answer vectors.

- Key = 64-bit seed. Counter = {sample index low, sample index high, draw block, 0}.
- Each **sample** owns its stream. Within one seed, distinct counters never repeat a block; statistical independence rests on Philox's tested quality (BigCrush), not on a proof.
- Four 32-bit words make two 53-bit uniforms in (0,1], `(x+1) * 2^-53`, so log(0) cannot occur. Box-Muller then gives two normals.
- Why not `std::normal_distribution`: its algorithm is implementation-defined, so results differ between standard libraries. Box-Muller uses a fixed number of draws and has no rejection loop, which suits a GPU.
- Cost acknowledged: slower than a ziggurat on a CPU. Tails stop near 8.6 sigma, which is irrelevant at these path counts.

**Reproducibility.** Samples are split into fixed chunks of 4096, and chunk accumulators merge in chunk order. Floating-point addition is not associative, so fixed chunks and fixed order give **bit-identical results for serial and any thread count** in the same binary. Across compilers, agreement is only to rounding, since libm differs. No `-ffast-math`.

## 6. Backends

- **Serial:** loops over chunks, merges in order.
- **Threaded:** `std::jthread` workers (0 = `hardware_concurrency()`, minimum 1) claim chunk indices from one `std::atomic`. This deliberately refines the brief's static split: P- and E-cores run at different speeds, so equal shares would leave P-cores waiting. The counter is the only contended mutable state. Each worker accumulates locally and writes each finished chunk to its own slot; reduction follows the joins. Worker exceptions are captured as `exception_ptr` and rethrown.
- **CUDA:** `ENABLE_CUDA=OFF` by default. The stub throws "CUDA backend not implemented"; ON compiles the stub with a warning and does not require nvcc. Kernel math avoids allocation, exceptions and virtual calls so it can later gain `__host__ __device__`.

## 7. Tests (GoogleTest v1.17.0, pinned by tag, FetchContent)

| Area | Check |
|---|---|
| Philox | Random123 vectors; moments; counter disjointness |
| Statistics | Merge equals a single pass; known small datasets |
| Analytic | Reference values, parity to 1e-12 relative, Greeks vs finite differences; geometric with N=1 equals Black-Scholes |
| European MC | Within 3 SE of Black-Scholes over several parameter sets |
| Geometric Asian MC | Within 3 SE of the closed form |
| Parity | MC C-P within 3 SE of e^(-qT)S0 - e^(-rT)K |
| Convergence | log SE vs log N slope -0.5 +/- 0.05, and RMSE of **true error** across seeds also falls at that rate |
| CI calibration | 400 seeds: 95% CI covers the true price in 92-98% of runs |
| Variance reduction | SE lower for each method; CV on arithmetic Asian at least 10x; estimates agree within combined SE |
| Backends | Bit-identical for threads 1, 2, 3, 8, max and a partial last chunk; different seeds differ but agree statistically |
| Edges | sigma = 0 and tiny, T = 0 and tiny, deep ITM/OTM, every invalid input |
| CLI | Parsing, unknown flags, malformed and overflowing numbers |

Seeds are fixed **in advance and never tuned**. Fixed seeds make tests deterministic. With many 3-SE checks an occasional honest failure is possible (0.27% each); investigate by rerunning over many seeds, and document any change. Zero-SE cases use an absolute tolerance.

Warnings: `/W4 /permissive-` or `-Wall -Wextra -Wpedantic`, with `MCPRICER_WARNINGS_AS_ERRORS`. Local: MSVC Debug and Release, GCC Release, MSVC AddressSanitizer. CI file: Windows MSVC, Ubuntu GCC, plus ThreadSanitizer on Ubuntu. CI and TSan are unverified until you push; the README will say so.

## 8. Benchmarks

- Workload: arithmetic Asian, 252 steps, at 1e4, 1e5 and 1e6 paths. Also European at 1e6 and 1e7.
- Serial, then threaded at 1, 2, 4, 8, 16, max. One warm-up, then the **median of 5** `steady_clock` runs, also recording min and max.
- CSV: timestamp, CPU, compiler, flags, backend, threads, option, steps, paths, median/min/max seconds, paths/s, speedup. A convergence CSV covers plain, antithetic, CV and both.
- Variance reduction is compared by **efficiency, 1/(SE^2 x time)**, so the extra cost counts.
- Reported limits: hybrid cores, hyper-threading and laptop thermals make linear scaling impossible; power state recorded; speedup is against SerialBackend.
- `plot.py` draws speedup with an ideal line, and convergence on log-log axes.

## 9. Milestones

| | Deliverable | Done when |
|---|---|---|
| M0 | Project repo, PLAN.md | Committed; then stop for "proceed" |
| M1 | CMake, presets, CI file, formatting, licence, smoke test | Builds and passes on MSVC and GCC |
| M2 | Market, validation, closed forms | Analytic tests pass |
| M3 | Philox, statistics, payoffs, kernel, serial engine | MC vs analytic, convergence and calibration pass |
| M4 | Antithetic and control variates | VR tests pass |
| M5 | ThreadedBackend, CUDA stub and option | Bit-identity tests pass; ASan clean |
| M6 | CLI | Your example command works; CLI tests pass |
| M7 | Benchmarks, CSV, plots | Run on this machine, CSV committed |
| M8 | README, LEARNING.md, CUDA_ROADMAP.md, cleanup | Every README command rerun from a fresh clone |
| M9 | CUDA backend | Only on explicit request |

Philox moves into M3 (M5 in the brief) so the serial engine is not rewritten. Commits carry a `Co-Authored-By: Claude` line by default; tell me if you want it omitted, and check the employer's policy on AI assistance.

## 10. Verification (every milestone)

```bash
cmake --preset msvc
cmake --build --preset msvc-release
ctest --preset msvc-release
cmake --preset gcc && cmake --build --preset gcc-release && ctest --preset gcc-release
```

Finally: clone fresh, build, test, run the example CLI command serial and threaded (prices must match exactly), and run the benchmark and plot script.

## 11. Non-goals

SIMD intrinsics, stochastic volatility, American options, Sobol, MC Greeks and Python bindings. These are listed as future work to keep the core to a few hundred lines.
