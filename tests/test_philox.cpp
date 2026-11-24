#include "mcpricer/philox.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <set>

namespace mcpricer {
namespace {

// Known-answer vectors published with the Random123 reference implementation.
TEST(Philox, MatchesRandom123Vectors) {
  EXPECT_EQ(philox::generate({0, 0, 0, 0}, {0, 0}),
            (philox::Counter{0x6627e8d5, 0xe169c58d, 0xbc57ac4c, 0x9b00dbd8}));
  EXPECT_EQ(
      philox::generate({0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}, {0xffffffff, 0xffffffff}),
      (philox::Counter{0x408f276d, 0x41c83b0e, 0xa20bc7c6, 0x6d5451fd}));
  EXPECT_EQ(
      philox::generate({0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344}, {0xa4093822, 0x299f31d0}),
      (philox::Counter{0xd16cfe09, 0x94fdcceb, 0x5001e420, 0x24126ea1}));
}

TEST(Philox, UniformsExcludeZeroAndIncludeOne) {
  EXPECT_GT(to_uniform(0, 0), 0.0);
  EXPECT_EQ(to_uniform(0xffffffff, 0xffffffff), 1.0);
}

TEST(NormalStream, IsReproducible) {
  NormalStream a(7, 3);
  NormalStream b(7, 3);
  for (int i = 0; i < 100; ++i) EXPECT_EQ(a.next(), b.next());
}

TEST(NormalStream, EverySeedAndSampleIndexBitMatters) {
  std::set<double> first_draws{NormalStream(0, 0).next()};
  for (int bit = 0; bit < 64; ++bit) {
    first_draws.insert(NormalStream(std::uint64_t{1} << bit, 0).next());
    first_draws.insert(NormalStream(0, std::uint64_t{1} << bit).next());
  }
  EXPECT_EQ(first_draws.size(), 129U);
}

// Tolerances are 4 standard errors of each sample moment. Draws are taken across streams as
// well as along them, as the pricer does.
TEST(NormalStream, HasStandardNormalMoments) {
  constexpr int kStreams = 100'000;
  constexpr int kDraws = 10;
  double m1 = 0.0;
  double m2 = 0.0;
  double m3 = 0.0;
  double m4 = 0.0;
  for (int s = 0; s < kStreams; ++s) {
    NormalStream stream(42, static_cast<std::uint64_t>(s));
    for (int d = 0; d < kDraws; ++d) {
      const double z = stream.next();
      m1 += z;
      m2 += z * z;
      m3 += z * z * z;
      m4 += z * z * z * z;
    }
  }
  const double n = kStreams * kDraws;
  const double tolerance = 4.0 / std::sqrt(n);
  EXPECT_NEAR(m1 / n, 0.0, tolerance);
  EXPECT_NEAR(m2 / n, 1.0, tolerance * std::sqrt(2.0));
  EXPECT_NEAR(m3 / n, 0.0, tolerance * std::sqrt(15.0));
  EXPECT_NEAR(m4 / n, 3.0, tolerance * std::sqrt(96.0));
}

TEST(NormalStream, NeighbouringStreamsAndDrawsAreUncorrelated) {
  constexpr int kStreams = 200'000;
  double across = 0.0;
  double along = 0.0;
  for (int s = 0; s < kStreams; ++s) {
    NormalStream current(42, static_cast<std::uint64_t>(s));
    NormalStream next(42, static_cast<std::uint64_t>(s) + 1);
    const double z = current.next();
    across += z * next.next();
    along += z * current.next();
  }
  const double tolerance = 4.0 / std::sqrt(static_cast<double>(kStreams));
  EXPECT_NEAR(across / kStreams, 0.0, tolerance);
  EXPECT_NEAR(along / kStreams, 0.0, tolerance);
}

}  // namespace
}  // namespace mcpricer
