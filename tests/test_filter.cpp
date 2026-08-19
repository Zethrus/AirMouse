#include <gtest/gtest.h>

#include "airmouse/pointer/filter.hpp"

using namespace airmouse;

TEST(Filter, FirstSamplePassthrough) {
  OneEuroFilter f(1.f, 0.007f, 1.f);
  const auto o = f.filter({3.f, 4.f}, 0.016f);
  EXPECT_FLOAT_EQ(o.x, 3.f);
  EXPECT_FLOAT_EQ(o.y, 4.f);
}

TEST(Filter, DeterministicSequence) {
  OneEuroFilter f(1.f, 0.007f, 1.f);
  Vec2 a = f.filter({0.f, 0.f}, 0.016f);
  Vec2 b = f.filter({1.f, 1.f}, 0.016f);
  OneEuroFilter g(1.f, 0.007f, 1.f);
  Vec2 c = g.filter({0.f, 0.f}, 0.016f);
  Vec2 d = g.filter({1.f, 1.f}, 0.016f);
  EXPECT_FLOAT_EQ(a.x, c.x);
  EXPECT_FLOAT_EQ(b.x, d.x);
  EXPECT_LT(b.x, 1.f);
  EXPECT_GT(b.x, 0.f);
}

TEST(Filter, DeadzoneHolds) {
  OneEuroFilter f(1.f, 0.007f, 1.f);
  f.set_deadzone(0.0015f);
  (void)f.filter({0.f, 0.f}, 0.016f);
  const auto held = f.filter({0.001f, 0.f}, 0.016f);
  EXPECT_FLOAT_EQ(held.x, 0.f);
  EXPECT_FLOAT_EQ(held.y, 0.f);
}

TEST(Filter, ResetPassthrough) {
  OneEuroFilter f(1.f, 0.007f, 1.f);
  (void)f.filter({0.f, 0.f}, 0.016f);
  (void)f.filter({1.f, 1.f}, 0.016f);
  f.reset();
  const auto o = f.filter({5.f, 6.f}, 0.016f);
  EXPECT_FLOAT_EQ(o.x, 5.f);
  EXPECT_FLOAT_EQ(o.y, 6.f);
}

TEST(Filter, TightenSoftens) {
  OneEuroFilter fast(1.f, 0.007f, 1.f);
  OneEuroFilter soft(1.f, 0.007f, 1.f);
  (void)fast.filter({0.f, 0.f}, 0.016f);
  (void)soft.filter({0.f, 0.f}, 0.016f);
  soft.tighten(0.6f);
  const auto a = fast.filter({1.f, 0.f}, 0.016f);
  const auto b = soft.filter({1.f, 0.f}, 0.016f);
  EXPECT_LT(b.x, a.x);
}
