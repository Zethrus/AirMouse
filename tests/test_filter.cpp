#include <gtest/gtest.h>

#include "cammouse/pointer/filter.hpp"

using namespace cammouse;

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
