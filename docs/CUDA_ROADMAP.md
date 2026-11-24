# CUDA roadmap

Status: **not implemented**. `ENABLE_CUDA=ON` builds a stub that throws. This document describes
how the GPU backend is intended to work and what to learn first. Performance is not predicted
here; it has to be measured.

## Why this problem suits a GPU

Paths are independent, every path runs the same instructions, and almost no memory is needed. A
GPU runs thousands of threads in groups that execute in lockstep, which is exactly this shape.

## What already exists for it

| Part | Why it carries over |
|---|---|
| `philox::generate` | A pure function on four integers. No state to keep or seed per thread |
| `NormalStream` | Box-Muller has no rejection loop, so threads in a warp stay in step |
| Payoff structs | Trivially copyable, no virtual functions, no allocation |
| `SimulationTask` | Small and trivially copyable: passed as a kernel argument |
| `BivariateAccumulator` | Mergeable, which is what a parallel reduction needs |
| `IExecutionBackend` | The engine, CLI, tests and benchmarks need no change |

## Planned kernel

One GPU thread simulates one sample (one path, or one antithetic pair):

```
for each GPU thread:
    stream = NormalStream(seed, first_sample + thread_index)
    evolve the path in registers, observing the payoff at each step
    result = (discounted payoff, discounted control)
```

Samples are launched in batches, say one million. A batch bounds device memory and keeps each
kernel short, because Windows resets a display GPU whose kernel runs too long.

### Random numbers

Two options, both Philox4x32-10:

1. **Mark our own functions `__host__ __device__`.** The GPU then uses the same integer streams as
   the CPU, so the two can be compared sample by sample. Preferred.
2. **cuRAND's device API** (`curandStatePhilox4_32_10_t`). Maintained by NVIDIA, but its mapping
   from seed and offset to counters is its own, so only statistical comparison is possible.

A useful test for option 1 is that raw integers match exactly. Prices will match only to rounding,
because `log`, `sin` and `exp` are implemented differently on the device.

### Memory layout

Paths are **not** stored: normals are generated, used and discarded in registers. If results have
to be written out, use a structure of arrays (`payoff[]`, `control[]`) rather than an array of
structures, so that neighbouring threads access adjacent memory.

### Reduction

1. Each thread handles a fixed group of samples and keeps a `BivariateAccumulator` in registers.
2. It writes the accumulator to a device array.
3. Accumulators are merged pairwise on the device, or copied back and merged on the host. The
   second is simpler and good enough when there are a few thousand of them.

Adding everything into one value with atomics would be slow and would make the order of additions,
and therefore the last bits, vary between runs.

### Precision

Consumer GPUs run `double` far slower than `float`. Start with `double` to validate, then try
`float` for paths with `double` accumulators, and test that the bias stays well below the standard
error.

## Build integration

- Toolkit: the development machine has an RTX 5050 Laptop GPU (compute capability 12.0, 8 GB).
  Check that the chosen CUDA 13.x release supports this GPU, the driver and the installed Visual
  Studio, since nvcc accepts only specific MSVC versions.
- CMake: `enable_language(CUDA)` inside `if(ENABLE_CUDA)`; add `src/cuda_backend.cu`.
- A macro, for instance `MCPRICER_HD`, that expands to `__host__ __device__` under nvcc and to
  nothing otherwise. Standard functions such as `std::max` need checking for device support.
- `std::variant` is resolved on the host: visit once, then launch a kernel template per payoff.

## Steps

1. Install the toolkit; compile and run a vector addition.
2. `MCPRICER_HD` on Philox; test that GPU integers equal CPU integers.
3. European call kernel; compare with Black-Scholes.
4. Asian payoffs, antithetics and the control.
5. Add the backend to the test suite (statistical agreement) and to the benchmark.
6. Optimise only after measuring: batch size, precision, samples per thread.

## What to learn

| Topic | Question to be able to answer |
|---|---|
| Threads, blocks, warps, grids | Why does branching differently within a warp cost time? |
| Memory hierarchy | Why are registers fast and global memory slow? |
| Host and device | What is copied when, and why does the transfer matter less here than usual? |
| Kernels and launch configuration | How are grid and block sizes chosen? |
| Reductions | Why not use one atomic sum? |
| Occupancy and profiling (Nsight) | How is it known what limits a kernel? |
| Timing | Why must the host synchronise before stopping the clock? |

Start with NVIDIA's *CUDA C++ Programming Guide*, the cuRAND documentation, and the Philox paper
(Salmon et al., "Parallel Random Numbers: As Easy as 1, 2, 3", SC11).
