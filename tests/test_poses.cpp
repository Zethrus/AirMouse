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
  const int mcp_ids[] = {kThumbMcp, kIndexMcp, kMiddleMcp, kRingMcp, kPinkyMcp};
  for (int i = 0; i < 5; ++i) {
    h.landmarks[tip_ids[i]] = {tips[i][0], tips[i][1], 0};
    h.landmarks[pip_ids[i]] = {tips[i][0], tips[i][1] + 0.18f, 0};
    h.landmarks[mcp_ids[i]] = {tips[i][0], tips[i][1] + 0.36f, 0};
  }
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  return h;
}

static HandFrame make_fist() {
  HandFrame h;
  h.landmarks[kWrist] = {0.5f, 0.7f, 0};
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  const int tips[] = {kThumbTip, kIndexTip, kMiddleTip, kRingTip, kPinkyTip};
  const int pips[] = {kThumbIp, kIndexPip, kMiddlePip, kRingPip, kPinkyPip};
  const int mcps[] = {kThumbMcp, kIndexMcp, kMiddleMcp, kRingMcp, kPinkyMcp};
  for (int i = 0; i < 5; ++i) {
    h.landmarks[mcps[i]] = {0.5f, 0.56f, 0};
    h.landmarks[pips[i]] = {0.5f, 0.50f, 0};
    h.landmarks[tips[i]] = {0.5f, 0.58f, 0};
  }
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  return h;
}

static HandFrame make_pointing() {
  HandFrame h = make_fist();
  h.landmarks[kIndexMcp] = {0.45f, 0.52f, 0};
  h.landmarks[kIndexPip] = {0.45f, 0.36f, 0};
  h.landmarks[kIndexDip] = {0.45f, 0.26f, 0};
  h.landmarks[kIndexTip] = {0.45f, 0.16f, 0};
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

TEST(Poses, PinchIgnoresZJitter) {
  auto h = make_open_palm();
  h.landmarks[kThumbTip] = {0.40f, 0.20f, 0.4f};
  h.landmarks[kIndexTip] = {0.41f, 0.21f, -0.4f};
  const auto f = analyze_pose(h);
  EXPECT_LT(f.pinch_index, 0.35f);
}

TEST(Poses, PointingIgnoresZJitter) {
  auto h = make_pointing();
  h.landmarks[kIndexTip].z = 0.5f;
  h.landmarks[kIndexPip].z = -0.4f;
  const auto f = analyze_pose(h);
  EXPECT_TRUE(f.index_up);
  EXPECT_FALSE(f.fist);
  EXPECT_FALSE(f.open_palm);
}

TEST(Poses, FingerHysteresisHoldsBorderline) {
  auto h = make_pointing();
  const auto up = analyze_pose(h);
  EXPECT_TRUE(up.index_up);

  // ~46° bend: cosine ≈ -0.70, between on (-0.88) and off (-0.55).
  h.landmarks[kIndexTip] = {0.564f, 0.248f, 0};
  const auto held = analyze_pose(h, up);
  EXPECT_TRUE(held.index_up);

  const auto cold = analyze_pose(h);
  EXPECT_FALSE(cold.index_up);
}
