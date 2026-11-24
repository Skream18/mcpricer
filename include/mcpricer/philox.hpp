#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace mcpricer {

/// Philox4x32-10 counter-based generator (Salmon et al., "Parallel Random Numbers: As Easy as
/// 1, 2, 3", SC11). A pure function: equal (counter, key) pairs always give equal output, so a
/// stream needs no state beyond its position, and any thread can produce any part of it.
namespace philox {

using Counter = std::array<std::uint32_t, 4>;
using Key = std::array<std::uint32_t, 2>;

[[nodiscard]] constexpr Counter generate(Counter counter, Key key) noexcept {
  constexpr std::uint64_t kMultiplier0 = 0xD2511F53;
  constexpr std::uint64_t kMultiplier1 = 0xCD9E8D57;
  constexpr std::uint32_t kWeyl0 = 0x9E3779B9;  // Golden ratio.
  constexpr std::uint32_t kWeyl1 = 0xBB67AE85;  // sqrt(3) - 1.
  constexpr int kRounds = 10;

  for (int round = 0; round < kRounds; ++round) {
    const std::uint64_t product0 = kMultiplier0 * counter[0];
    const std::uint64_t product1 = kMultiplier1 * counter[2];
    counter = {static_cast<std::uint32_t>(product1 >> 32) ^ counter[1] ^ key[0],
               static_cast<std::uint32_t>(product1),
               static_cast<std::uint32_t>(product0 >> 32) ^ counter[3] ^ key[1],
               static_cast<std::uint32_t>(product0)};
    key[0] += kWeyl0;
    key[1] += kWeyl1;
  }
  return counter;
}

}  // namespace philox

/// Maps 64 random bits to a uniform double in (0, 1]. Zero is excluded because Box-Muller takes
/// its logarithm. A double holds 53 significant bits, so the remaining 11 are dropped.
[[nodiscard]] constexpr double to_uniform(std::uint32_t low, std::uint32_t high) noexcept {
  const std::uint64_t bits = (static_cast<std::uint64_t>(high) << 32) | low;
  return static_cast<double>((bits >> 11) + 1) * 0x1.0p-53;
}

/// Independent standard normals for one Monte Carlo sample. The stream is selected by
/// (seed, sample index), never by thread, so results do not depend on how work is divided.
class NormalStream {
 public:
  NormalStream(std::uint64_t seed, std::uint64_t sample_index) noexcept
      : key_{static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(seed >> 32)},
        counter_{static_cast<std::uint32_t>(sample_index),
                 static_cast<std::uint32_t>(sample_index >> 32), 0, 0} {}

  // Box-Muller rather than std::normal_distribution: the standard leaves that algorithm to the
  // library, so the same seed would give different prices with different compilers. It also
  // always consumes the same number of draws, which suits a GPU.
  [[nodiscard]] double next() noexcept {
    if (has_spare_) {
      has_spare_ = false;
      return spare_;
    }
    const philox::Counter bits = philox::generate(counter_, key_);
    ++counter_[2];
    const double radius = std::sqrt(-2.0 * std::log(to_uniform(bits[0], bits[1])));
    const double angle = 2.0 * std::numbers::pi * to_uniform(bits[2], bits[3]);
    spare_ = radius * std::sin(angle);
    has_spare_ = true;
    return radius * std::cos(angle);
  }

 private:
  philox::Key key_;
  philox::Counter counter_;
  double spare_ = 0.0;
  bool has_spare_ = false;
};

}  // namespace mcpricer
