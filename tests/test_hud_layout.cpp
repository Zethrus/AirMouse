#include <gtest/gtest.h>

#include "airmouse/ui/hud_layout.hpp"

using namespace airmouse;

namespace {

constexpr WorkArea kDesk{0.f, 0.f, 1920.f, 1080.f};
constexpr float kW = static_cast<float>(theme::hud::w);
constexpr float kH = static_cast<float>(theme::hud::h);

}  // namespace

TEST(HudLayout, DefaultIsTopRight) {
  const auto p = default_position(kDesk, kW, kH, 1.f);
  EXPECT_FLOAT_EQ(p.x, 1920.f - kW - theme::hud::pad);
  EXPECT_FLOAT_EQ(p.y, static_cast<float>(theme::hud::pad));
}

TEST(HudLayout, ClampKeepsHudOnScreen) {
  const auto p = clamp_position(kDesk, 4000.f, -40.f, kW, kH);
  EXPECT_FLOAT_EQ(p.x, 1920.f - kW);
  EXPECT_FLOAT_EQ(p.y, 0.f);
}

TEST(HudLayout, SnapCornersAndEdges) {
  const float pad = static_cast<float>(theme::hud::pad);
  const auto corner = snap_target(kDesk, pad + 8.f, pad + 6.f, kW, kH, 1.f);
  EXPECT_FLOAT_EQ(corner.x, pad);
  EXPECT_FLOAT_EQ(corner.y, pad);

  const auto edge = snap_target(kDesk, 900.f, pad + 4.f, kW, kH, 1.f);
  EXPECT_FLOAT_EQ(edge.x, 900.f);
  EXPECT_FLOAT_EQ(edge.y, pad);
}

TEST(HudLayout, NoSnapInTheOpen) {
  const auto p = snap_target(kDesk, 640.f, 400.f, kW, kH, 1.f);
  EXPECT_FLOAT_EQ(p.x, 640.f);
  EXPECT_FLOAT_EQ(p.y, 400.f);
}

TEST(HudLayout, GripHit) {
  EXPECT_TRUE(in_grip(10.f, 8.f, kW, 1.f));
  EXPECT_FALSE(in_grip(10.f, static_cast<float>(theme::hud::grip_h) + 1.f, kW, 1.f));
  EXPECT_FALSE(in_grip(-1.f, 4.f, kW, 1.f));
}

TEST(HudLayout, OffscreenRestore) {
  EXPECT_TRUE(offscreen(kDesk, 2000.f, 10.f, kW, kH));
  EXPECT_TRUE(offscreen(kDesk, -400.f, -400.f, kW, kH));
  EXPECT_FALSE(offscreen(kDesk, 100.f, 100.f, kW, kH));
}

TEST(HudLayout, EaseAndSpringConverge) {
  float v = 0.f;
  for (int i = 0; i < 40; ++i) v = ease_exp(v, 1.f, 0.016f, theme::motion::hover_tau);
  EXPECT_GT(v, 0.95f);

  float pos = 0.f;
  float vel = 0.f;
  for (int i = 0; i < 80; ++i) spring_step(pos, vel, 100.f, 0.016f, theme::motion::spring_tau);
  EXPECT_NEAR(pos, 100.f, 1.0);
  EXPECT_TRUE(spring_settled(pos, vel, 100.f));
}
