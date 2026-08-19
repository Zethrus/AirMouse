#include <cmath>

#include <gtest/gtest.h>

#include "airmouse/tracking/landmark_filter.hpp"

using namespace airmouse;

static HandFrame at(float x, float y) {
  HandFrame h;
  for (int i = 0; i < kLandmarkCount; ++i) {
    h.landmarks[i] = {x, y, 0.3f};
  }
  h.presence = 0.9f;
  return h;
}

TEST(LandmarkFilter, FirstSamplePassthrough) {
  LandmarkFilter f;
  const auto o = f.filter(at(0.4f, 0.6f), 0.016f);
  EXPECT_FLOAT_EQ(o.landmarks[kIndexTip].x, 0.4f);
  EXPECT_FLOAT_EQ(o.landmarks[kIndexTip].y, 0.6f);
  EXPECT_FLOAT_EQ(o.landmarks[kIndexTip].z, 0.3f);
  EXPECT_FLOAT_EQ(o.presence, 0.9f);
}

TEST(LandmarkFilter, DampsJitter) {
  LandmarkFilter f(1.2f, 0.007f, 1.f);
  (void)f.filter(at(0.5f, 0.5f), 0.016f);
  float max_dev = 0.f;
  for (int i = 0; i < 20; ++i) {
    const float n = (i % 2 == 0) ? 0.02f : -0.02f;
    const auto o = f.filter(at(0.5f + n, 0.5f), 0.016f);
    max_dev = std::max(max_dev, std::fabs(o.landmarks[kIndexTip].x - 0.5f));
  }
  EXPECT_LT(max_dev, 0.02f);
}

TEST(LandmarkFilter, ResetPassthrough) {
  LandmarkFilter f;
  (void)f.filter(at(0.f, 0.f), 0.016f);
  (void)f.filter(at(1.f, 1.f), 0.016f);
  f.reset();
  const auto o = f.filter(at(0.2f, 0.3f), 0.016f);
  EXPECT_FLOAT_EQ(o.landmarks[kWrist].x, 0.2f);
  EXPECT_FLOAT_EQ(o.landmarks[kWrist].y, 0.3f);
}
