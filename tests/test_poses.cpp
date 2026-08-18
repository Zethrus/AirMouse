#include <gtest/gtest.h>

#include "airmouse/gestures/poses.hpp"

using namespace airmouse;

static HandFrame make_open_palm() {
  HandFrame h;
  h.landmarks[kWrist] = {0.5f, 0.8f, 0};
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  const float tips[][2] = {
      {0.35f, 0.15f}, {0.45f, 0.10f}, {0.55f, 0.10f}, {0.65f, 0.15f}, {0.72f, 0.22f}};
  const int tip_ids[] = {kThumbTip, kIndexTip, kMiddleTip, kRingTip, kPinkyTip};
  const int pip_ids[] = {kThumbIp, kIndexPip, kMiddlePip, kRingPip, kPinkyPip};
  for (int i = 0; i < 5; ++i) {
    h.landmarks[tip_ids[i]] = {tips[i][0], tips[i][1], 0};
    h.landmarks[pip_ids[i]] = {tips[i][0], tips[i][1] + 0.18f, 0};
  }
  return h;
}

static HandFrame make_fist() {
  HandFrame h;
  h.landmarks[kWrist] = {0.5f, 0.7f, 0};
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  const int tips[] = {kThumbTip, kIndexTip, kMiddleTip, kRingTip, kPinkyTip};
  const int pips[] = {kThumbIp, kIndexPip, kMiddlePip, kRingPip, kPinkyPip};
  for (int i = 0; i < 5; ++i) {
    h.landmarks[tips[i]] = {0.5f, 0.58f, 0};
    h.landmarks[pips[i]] = {0.5f, 0.50f, 0};
  }
  return h;
}

TEST(Poses, OpenPalm) {
  const auto f = analyze_pose(make_open_palm());
  EXPECT_TRUE(f.open_palm);
  EXPECT_FALSE(f.fist);
}

TEST(Poses, Fist) {
  const auto f = analyze_pose(make_fist());
  EXPECT_TRUE(f.fist);
  EXPECT_FALSE(f.open_palm);
}

TEST(Poses, PinchCloses) {
  auto h = make_open_palm();
  h.landmarks[kThumbTip] = {0.40f, 0.20f, 0};
  h.landmarks[kIndexTip] = {0.41f, 0.21f, 0};
  const auto f = analyze_pose(h);
  EXPECT_LT(f.pinch_index, 0.35f);
}
