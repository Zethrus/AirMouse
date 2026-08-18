#include <gtest/gtest.h>

#include "airmouse/pointer/mapper.hpp"

using namespace airmouse;

TEST(Mapper, Corners) {
  Mapper m;
  m.control_box = 1.0f;
  ScreenGeometry s{0, 0, 1000, 500};
  const auto tl = m.map(0.f, 0.f, s);
  EXPECT_NEAR(tl.x, 0.f, 0.01f);
  EXPECT_NEAR(tl.y, 0.f, 0.01f);
  const auto br = m.map(1.f, 1.f, s);
  EXPECT_NEAR(br.x, 1000.f, 0.01f);
  EXPECT_NEAR(br.y, 500.f, 0.01f);
}

TEST(Mapper, ControlBoxMargins) {
  Mapper m;
  m.control_box = 0.5f;
  ScreenGeometry s{10, 20, 100, 100};
  const auto mid = m.map(0.5f, 0.5f, s);
  EXPECT_NEAR(mid.x, 60.f, 0.5f);
  EXPECT_NEAR(mid.y, 70.f, 0.5f);
}
