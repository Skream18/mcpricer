# Learning guide

How this project works and why, in plain language, with interview questions at the end. Read it
with the code open.

## 1. The journey of one price

1. [`apps/cli_parser.cpp`](../apps/cli_parser.cpp) turns text into structs.
2. `price_monte_carlo` in [`src/monte_carlo.cpp`](../src/monte_carlo.cpp) validates them and
   precomputes constants into a `SimulationTask`.
3. A backend ([`serial`](../src/serial_backend.cpp) or [`threaded`](../src/threaded_backend.cpp))
   splits the samples into chunks of 1024.
4. `simulate_chunk` in [`src/path_kernel.cpp`](../src/path_kernel.cpp) finds the payoff type once,
   then `simulate_samples` in [`path_kernel.hpp`](../include/mcpricer/path_kernel.hpp) runs the loop.
5. Each sample draws normals ([`philox.hpp`](../include/mcpricer/philox.hpp)), evolves a path,
   evaluates a payoff ([`payoffs.hpp`](../include/mcpricer/payoffs.hpp)) and updates running
   statistics ([`statistics.hpp`](../include/mcpricer/statistics.hpp)).
6. Chunk statistics are merged in order, and
   [`src/statistics.cpp`](../src/statistics.cpp) turns them into a price and an error.

If you can explain these six steps without notes, you know the project.

## 2. The finance

**Geometric Brownian motion** says that over a short time the *percentage* change of a price is a
drift plus random noise. Prices stay positive, and log prices are normally distributed.

**Risk-neutral pricing.** A European option can be hedged by trading the stock. The hedged position
is riskless, so it must earn the risk-free rate, and the stock's real expected return drops out.
We can therefore pretend the stock grows at r − q, average the payoff, and discount at r.

**Why −σ²/2 in the log drift?** We need E[S(T)] = S0 e^((r−q)T). The exponential is convex, so
noise in the log raises the mean of S by e^(σ²T/2) (Jensen's inequality). The term cancels that.

**Asian options** pay on an average price. The *geometric* average of lognormals is lognormal, so
it has a Black-Scholes-like formula. The *arithmetic* average is not lognormal and has no simple
closed form, which is why Monte Carlo is useful. See `geometric_asian_price` in
[`src/analytic.cpp`](../src/analytic.cpp).

## 3. The statistics

**Monte Carlo** estimates an expectation by a sample mean. By the central limit theorem the error
is approximately normal with standard deviation σ_X / √n, whatever the number of time steps. That
independence from dimension is its strength: a grid in 252 dimensions is impossible.

**Standard error vs standard deviation.** The standard deviation describes one payoff; the
standard error describes the *average* of n of them.

**A 95% confidence interval** does not mean a 95% chance that the true price is inside this one
interval. It means that about 95% of intervals built this way contain the true price. The
calibration tests check precisely that, over 400 seeds.

**Welford's method.** Computing Σx² − n·mean² subtracts two huge, nearly equal numbers and loses
digits. Welford updates the mean and the squared deviations one sample at a time. Merging two
accumulators (Chan et al.) adds a term for the distance between their means.

## 4. Variance reduction

**Antithetic.** If Z gives a high payoff, −Z gives a low one, so averaging them cancels noise.
Var((A+B)/2) = Var(A)(1 + ρ)/2. Compared with two independent paths, which give Var(A)/2, it helps
when ρ < 0. Here it also halves the random numbers needed.

**Control variate.** Suppose the geometric Asian comes out 0.10 too high on our paths. The
arithmetic Asian is probably about 0.10 too high as well, for the same reason, so subtract it.
β is how strongly one moves with the other, and is the slope of a regression. The remaining
variance is 1 − ρ² of the original. For the Asian benchmark the standard error falls about 36-fold;
matching that with plain Monte Carlo would take about 1300 times as many paths.

**Combining them** is not automatically better. For our Asian, antithetics removed noise the
control had already removed. For the European, they removed different noise and the combination
was much better.

## 5. Random numbers

**A normal generator** like Mersenne Twister is a machine with state: to get number one million you
run it one million times. **Philox** is a function: give it a position and a key and it returns the
scrambled number at that position. It multiplies, swaps and XORs ten times, like a small cipher.

Our position is (sample index, draw block) and the key is the seed. Sample 7 gets the same numbers
whichever thread runs it. That one decision makes results independent of the thread count.

**Why not seed one Mersenne Twister per thread with 1, 2, 3...?** Nothing guarantees that streams
from nearby seeds do not overlap or correlate, and the answer changes with the thread count.

**Box-Muller** turns two uniforms into two normals using polar coordinates: √(−2 ln U₁) is the
radius, 2πU₂ the angle. U₁ must not be zero, hence the interval (0, 1] in `to_uniform`.

## 6. The C++ and the threads

**Static inside, dynamic outside.** Choosing a backend happens once, so a virtual function is fine.
The payoff is used at each of 252 steps of each of a million paths. As a template it is inlined.

**A concept** states what a type must provide, and the compiler checks it. `std::variant` holds one
of a known list of types without heap allocation; `std::visit` picks the right code.

**Why the results are bit-identical.** In floating point (a+b)+c may differ from a+(b+c). If threads
added results as they finished, the last digits would vary. We fix the chunks and the merging order.

**No data races.** Threads share one atomic counter. `fetch_add` gives each chunk to exactly one
thread, which writes only that chunk's slot. Joining the threads makes all writes visible. The
counter is *relaxed* because it protects no other data.

**False sharing** is when threads write different variables on one cache line and slow each other
down. Each worker accumulates in its own local object and writes the shared vector once per chunk.

**RAII.** `std::jthread` joins in its destructor, even if an exception is thrown.

## 7. Changes made while building

- The plan had an engine class holding a backend reference. A free function needs no state and
  cannot hold a dangling reference.
- The plan transported exceptions out of workers. The kernel cannot throw, so it is `noexcept`.
- Chunks went from 4096 to 1024, since 10,000 paths made only three chunks.
- A test threshold for the European put control was guessed as 0.6 and failed at 0.62. Theory
  (√(1−ρ²)) also gives 0.62, so the test now compares with theory instead of a guess.
- CMake reported the benchmark CPU as a Pentium III. The name now comes from the registry.

## 8. Interview questions

**Why does the error scale as 1/√N?** The variance of a mean of N independent samples is σ²/N, and
the error is its square root. Ten times more accuracy costs a hundred times more work. That is
why variance reduction matters.

**Why antithetic?** The payoffs are monotone in Z, so Z and −Z give negatively correlated payoffs.
For a payoff symmetric in Z, such as a straddle near the money, it would be useless or harmful.

**Why must pairs count as one sample?** The two halves are dependent. The formula s/√n assumes
independence, so treating them as 2n samples would report a wrong error.

**Is your control variate biased?** Slightly, as β comes from the same samples: O(1/n). The
standard error is O(1/√n), so the bias vanishes faster. A pilot run would remove it at extra cost.

**Why is the geometric Asian such a good control?** It averages the same prices on the same path.
The correlation is nearly one, and the formula gives its exact mean.

**Why reject a control for the geometric Asian itself?** It would be its own control. The
result would be the closed form, and the comparison with the closed form would prove nothing.

**How do you avoid RNG correlation across threads?** Streams belong to samples. Within a seed, each
(sample, block) is a distinct counter, so no block is reused. That outputs *look* independent is an
empirical property: Philox passes BigCrush. I also test neighbouring streams for correlation.

**How do you know your Philox is right?** It reproduces the published Random123 test vectors.

**Why no discretisation error?** GBM has an exact solution, which I simulate. With Heston or local
volatility I would need Euler or Milstein steps and would have to study that bias.

**Serial and threaded match exactly. Would a GPU?** The integers can match exactly if the same
Philox code is used. Prices would not match to the last bit, since math functions round differently.

**How would this map to a GPU?** One thread per sample, the same Philox with the sample index as
counter, paths kept in registers, results reduced per group and then merged. See
[CUDA_ROADMAP.md](CUDA_ROADMAP.md). I have not implemented it.

**Why is the speedup 15 and not 24?** The CPU has 8 fast and 8 slower cores; the remaining 8
threads are hyper-threads sharing the fast ones. Up to 8 threads scaling is near ideal.

**Why dynamic scheduling?** With equal shares everyone waits for the slowest core. Claiming small
chunks keeps all cores busy, and the cost is one atomic increment per 1024 samples.

**How do you know the standard error is right?** I run 400 seeds and count how often the interval
contains the exact price. It must be near 95%, for every variance reduction method.

**Your tests are random. Are they flaky?** Seeds are fixed, so each build is deterministic. They
were not chosen to pass. A correct engine fails a 3-standard-error check with probability 0.27%;
if one failed I would rerun over many seeds before touching anything.

**What happens far out of the money?** Few or no paths pay off, so the estimate and its error are
unreliable: zero hits report zero error. Importance sampling would fix it.

**Why `double`?** Safe accumulation and no rounding concerns. `float` is worth testing on a GPU.

**What would you do next?** Profile the kernel. Likely candidates are vectorising Box-Muller and
`exp`, then Sobol sequences and the CUDA backend.

**Did you use AI to build this?** Answer truthfully: yes, it was built with an AI assistant and the
commits say so. Be ready to explain the decisions, and to change the code live.

## 9. Practice

1. Price with `--paths` 10,000 and then 40,000. Does the standard error halve?
2. Compare `--backend serial` with `--threads 3`. Are all digits equal?
3. Derive the geometric Asian mean and variance by hand, then compare with the code.
4. Add a digital option payoff. Which files change? (`market.hpp`, `payoffs.hpp`, `payoffs.cpp`,
   `analytic.cpp`, the control mean in `monte_carlo.cpp`, the CLI and the tests; no backend.)
5. Break the merging order in the threaded backend and see which test fails.
