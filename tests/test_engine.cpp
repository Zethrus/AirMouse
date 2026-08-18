#include <gtest/gtest.h>

#include "cammouse/gestures/engine.hpp"
#include "cammouse/gestures/poses.hpp"

using namespace cammouse;

static HandFrame pointing() {
  HandFrame h;
  h.landmarks[kWrist] = {0.5f, 0.8f, 0};
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  h.landmarks[kIndexTip] = {0.45f, 0.20f, 0};
  h.landmarks[kIndexPip] = {0.45f, 0.38f, 0};
  h.landmarks[kMiddleTip] = {0.55f, 0.50f, 0};
  h.landmarks[kMiddlePip] = {0.55f, 0.42f, 0};
  h.landmarks[kRingTip] = {0.60f, 0.52f, 0};
  h.landmarks[kRingPip] = {0.60f, 0.44f, 0};
  h.landmarks[kPinkyTip] = {0.65f, 0.53f, 0};
  h.landmarks[kPinkyPip] = {0.65f, 0.45f, 0};
  h.landmarks[kThumbTip] = {0.30f, 0.50f, 0};
  h.presence = 1.f;
  return h;
}

static HandFrame pinch_from(HandFrame h) {
  h.landmarks[kThumbTip] = h.landmarks[kIndexTip];
  h.landmarks[kThumbTip].x += 0.005f;
  return h;
}

TEST(Engine, LostReleases) {
  GestureEngine e(default_config());
  (void)e.update(pointing(), 0);
  const auto cmd = e.update(std::nullopt, 16);
  EXPECT_TRUE(cmd.release_all);
  EXPECT_EQ(cmd.pose, PoseName::Lost);
}

TEST(Engine, LeftClick) {
  GestureEngine e(default_config());
  auto hand = pointing();
  (void)e.update(hand, 0);
  auto p = pinch_from(hand);
  (void)e.update(p, 10);
  const auto cmd = e.update(hand, 120);
  EXPECT_EQ(cmd.press, Button::Left);
  EXPECT_EQ(cmd.release, Button::Left);
}

TEST(Engine, DragAfterHold) {
  GestureEngine e(default_config());
  auto p = pinch_from(pointing());
  (void)e.update(p, 0);
  const auto held = e.update(p, 250);
  EXPECT_EQ(held.pose, PoseName::Drag);
  EXPECT_EQ(held.press, Button::Left);
}

TEST(Engine, FistLocks) {
  GestureEngine e(default_config());
  HandFrame fist;
  fist.landmarks[kWrist] = {0.5f, 0.7f, 0};
  fist.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  for (int i = 1; i < kLandmarkCount; ++i) {
    fist.landmarks[i] = {0.5f, 0.58f, 0};
  }
  (void)e.update(fist, 0);
  const auto cmd = e.update(fist, 800);
  EXPECT_TRUE(e.locked());
  EXPECT_TRUE(cmd.release_all);
}
