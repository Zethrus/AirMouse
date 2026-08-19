#include <gtest/gtest.h>

#include "airmouse/ui/hud_drag.hpp"

using namespace airmouse;

namespace {

constexpr WorkArea kDesk{0.f, 0.f, 1920.f, 1080.f};

HudDrag make_drag() {
  HudDrag d;
  d.set_metrics(static_cast<float>(theme::hud::w), static_cast<float>(theme::hud::h), 1.f);
  HudConfig cfg;
  d.place(cfg, kDesk);
  return d;
}

}  // namespace

TEST(HudDrag, UnplacedDefaultsTopRight) {
  HudDrag d = make_drag();
  EXPECT_FALSE(d.placed);
  EXPECT_NEAR(d.x, 1920.f - theme::hud::w - theme::hud::pad, 0.51);
  EXPECT_NEAR(d.y, static_cast<float>(theme::hud::pad), 0.51);
}

TEST(HudDrag, PressOutsideGripIgnored) {
  HudDrag d = make_drag();
  const float ox = d.x;
  const float oy = d.y;
  EXPECT_FALSE(d.on_press(d.x + 20.f, d.y + 80.f));
  EXPECT_EQ(d.phase, HudDrag::Phase::Idle);
  EXPECT_FLOAT_EQ(d.x, ox);
  EXPECT_FLOAT_EQ(d.y, oy);
}

TEST(HudDrag, DragKeepsGrabOffset) {
  HudDrag d = make_drag();
  ASSERT_TRUE(d.on_press(d.x + 12.f, d.y + 8.f));
  EXPECT_EQ(d.phase, HudDrag::Phase::Dragging);
  d.on_move(400.f + 12.f, 300.f + 8.f, kDesk);
  EXPECT_NEAR(d.x, 400.f, 0.51);
  EXPECT_NEAR(d.y, 300.f, 0.51);
}

TEST(HudDrag, ReleaseNearEdgeSnaps) {
  HudDrag d = make_drag();
  ASSERT_TRUE(d.on_press(d.x + 4.f, d.y + 4.f));
  d.on_move(theme::hud::pad + 10.f + 4.f, theme::hud::pad + 6.f + 4.f, kDesk);
  EXPECT_TRUE(d.on_release(kDesk));
  EXPECT_TRUE(d.placed);
  EXPECT_EQ(d.phase, HudDrag::Phase::Settling);
  for (int i = 0; i < 80; ++i) d.tick(0.016f, kDesk);
  EXPECT_NEAR(d.x, static_cast<float>(theme::hud::pad), 1.0);
  EXPECT_NEAR(d.y, static_cast<float>(theme::hud::pad), 1.0);
  EXPECT_NE(d.phase, HudDrag::Phase::Settling);
}

TEST(HudDrag, ReleaseInOpenStays) {
  HudDrag d = make_drag();
  ASSERT_TRUE(d.on_press(d.x + 4.f, d.y + 4.f));
  d.on_move(640.f + 4.f, 400.f + 4.f, kDesk);
  EXPECT_TRUE(d.on_release(kDesk));
  EXPECT_EQ(d.phase, HudDrag::Phase::Hover);
  EXPECT_NEAR(d.x, 640.f, 0.51);
  EXPECT_NEAR(d.y, 400.f, 0.51);
}

TEST(HudDrag, OffscreenConfigRestoresDefault) {
  HudDrag d = make_drag();
  HudConfig cfg;
  cfg.placed = true;
  cfg.x = 8000;
  cfg.y = 20;
  d.place(cfg, kDesk);
  EXPECT_FALSE(d.placed);
  EXPECT_NEAR(d.x, 1920.f - theme::hud::w - theme::hud::pad, 0.51);
}
