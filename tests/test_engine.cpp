#include <gtest/gtest.h>

#include "airmouse/gestures/engine.hpp"
#include "airmouse/gestures/poses.hpp"

using namespace airmouse;

static HandFrame pointing() {
  HandFrame h;
  h.landmarks[kWrist] = {0.5f, 0.8f, 0};
  h.landmarks[kMiddleMcp] = {0.5f, 0.55f, 0};
  h.landmarks[kIndexMcp] = {0.45f, 0.52f, 0};
  h.landmarks[kIndexPip] = {0.45f, 0.36f, 0};
  h.landmarks[kIndexDip] = {0.45f, 0.26f, 0};
  h.landmarks[kIndexTip] = {0.45f, 0.16f, 0};

  auto curl = [&](int mcp, int pip, int dip, int tip, float x) {
    h.landmarks[mcp] = {x, 0.55f, 0};
    h.landmarks[pip] = {x, 0.50f, 0};
    h.landmarks[dip] = {x, 0.52f, 0};
    h.landmarks[tip] = {x, 0.54f, 0};
  };
  curl(kMiddleMcp, kMiddlePip, kMiddleDip, kMiddleTip, 0.52f);
  curl(kRingMcp, kRingPip, kRingDip, kRingTip, 0.58f);
  curl(kPinkyMcp, kPinkyPip, kPinkyDip, kPinkyTip, 0.64f);

  h.landmarks[kThumbCmc] = {0.42f, 0.62f, 0};
  h.landmarks[kThumbMcp] = {0.36f, 0.58f, 0};
  h.landmarks[kThumbIp] = {0.32f, 0.54f, 0};
  h.landmarks[kThumbTip] = {0.28f, 0.50f, 0};
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
  const auto cmd = e.update(std::nullopt, 200);
  EXPECT_TRUE(cmd.release_all);
  EXPECT_EQ(cmd.pose, PoseName::Lost);
}

TEST(Engine, SingleMissDoesNotLose) {
  GestureEngine e(default_config());
  (void)e.update(pointing(), 0);
  const auto miss = e.update(std::nullopt, 16);
  EXPECT_FALSE(miss.release_all);
  EXPECT_EQ(miss.pose, PoseName::Point);
  const auto lost = e.update(std::nullopt, 200);
  EXPECT_TRUE(lost.release_all);
  EXPECT_EQ(lost.pose, PoseName::Lost);
}

TEST(Engine, LeftClick) {
  GestureEngine e(default_config());
  auto hand = pointing();
  (void)e.update(hand, 0);
  auto p = pinch_from(hand);
  (void)e.update(p, 16);
  (void)e.update(p, 32);
  (void)e.update(hand, 132);
  const auto cmd = e.update(hand, 148);
  EXPECT_EQ(cmd.press, Button::Left);
  EXPECT_EQ(cmd.release, Button::Left);
}

TEST(Engine, OneFramePinchDoesNotClick) {
  GestureEngine e(default_config());
  auto hand = pointing();
  (void)e.update(hand, 0);
  (void)e.update(pinch_from(hand), 16);
  const auto cmd = e.update(hand, 32);
  EXPECT_EQ(cmd.press, Button::Idle);
  EXPECT_EQ(cmd.release, Button::Idle);
  EXPECT_NE(cmd.pose, PoseName::Pinch);
}

TEST(Engine, DragAfterHold) {
  GestureEngine e(default_config());
  auto p = pinch_from(pointing());
  (void)e.update(p, 0);
  (void)e.update(p, 16);
  const auto held = e.update(p, 400);
  EXPECT_EQ(held.pose, PoseName::Drag);
  EXPECT_EQ(held.press, Button::Left);
}

TEST(Engine, PinchFreezesCursor) {
  GestureEngine e(default_config());
  auto hand = pointing();
  const auto pointed = e.update(hand, 0);
  auto p = pinch_from(hand);
  p.landmarks[kIndexPip].x += 0.08f;
  p.landmarks[kIndexTip].x += 0.08f;
  (void)e.update(p, 16);
  const auto pinched = e.update(p, 32);
  EXPECT_EQ(pinched.pose, PoseName::Pinch);
  EXPECT_NEAR(pinched.nx, pointed.nx, 0.002f);
  EXPECT_NEAR(pinched.ny, pointed.ny, 0.002f);
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
