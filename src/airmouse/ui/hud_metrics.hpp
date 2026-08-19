#pragma once

#include "airmouse/ui/theme.hpp"
#include "airmouse/types.hpp"

namespace airmouse {

struct HudPoint {
  float x = 0;
  float y = 0;
};

struct HudBox {
  float x = 0;
  float y = 0;
  float w = 0;
  float h = 0;
};

struct HudMetrics {
  float scale = 1.f;
  float w = 0;
  float h = 0;
  float radius = 0;
  HudBox chassis{};
  HudBox grip{};
  HudPoint dots[6]{};
  HudPoint wordmark{};
  HudPoint pip{};
  HudPoint status{};
  float rule_y = 0;
  HudBox well{};
  float bracket = 0;
  HudPoint grid0{};
  float grid_dx = 0;
  float grid_dy = 0;
  HudPoint pose{};
  HudPoint seg0{};
  HudPoint conf{};
  HudBox chip{};
};

inline constexpr int kHandBones[][2] = {
    {0, 1},  {1, 2},  {2, 3},  {3, 4},   {0, 5},  {5, 6},  {6, 7},  {7, 8},
    {0, 9},  {9, 10}, {10, 11}, {11, 12}, {0, 13}, {13, 14}, {14, 15}, {15, 16},
    {0, 17}, {17, 18}, {18, 19}, {19, 20}, {5, 9},  {9, 13}, {13, 17},
};

inline bool index_ray(int a, int b) {
  return (a == kIndexMcp && b == kIndexPip) || (a == kIndexPip && b == kIndexDip) ||
         (a == kIndexDip && b == kIndexTip) || (a == 0 && b == kIndexMcp);
}

inline HudMetrics hud_metrics(float scale) {
  HudMetrics m;
  m.scale = scale;
  m.w = static_cast<float>(theme::hud::w) * scale;
  m.h = static_cast<float>(theme::hud::h) * scale;
  m.radius = static_cast<float>(theme::radius::panel) * scale;
  m.chassis = {0.5f * scale, 0.5f * scale, m.w - scale, m.h - scale};
  m.grip = {0.f, 0.f, m.w, static_cast<float>(theme::hud::grip_h) * scale};

  const float gx = static_cast<float>(theme::space::sm) * scale;
  const float gy = 6.f * scale;
  const float gap = 3.5f * scale;
  int di = 0;
  for (int col = 0; col < 2; ++col) {
    for (int row = 0; row < 3; ++row) {
      m.dots[di++] = {gx + static_cast<float>(col) * gap, gy + static_cast<float>(row) * gap};
    }
  }

  m.wordmark = {26.f * scale, 14.f * scale};
  m.pip = {m.w - 48.f * scale, 11.f * scale};
  m.status = {m.w - 42.f * scale, 14.f * scale};
  m.rule_y = m.grip.h;

  const float inset = static_cast<float>(theme::hud::well_inset) * scale;
  const float footer = static_cast<float>(theme::hud::footer_h) * scale;
  m.well = {inset, m.rule_y + 6.f * scale, m.w - inset * 2.f, m.h - m.rule_y - footer - 8.f * scale};
  m.bracket = static_cast<float>(theme::hud::bracket) * scale;
  m.grid0 = {m.well.x, m.well.y};
  m.grid_dx = m.well.w / static_cast<float>(theme::hud::grid_x - 1);
  m.grid_dy = m.well.h / static_cast<float>(theme::hud::grid_y - 1);

  m.pose = {static_cast<float>(theme::space::lg) * scale, m.h - 8.f * scale};
  const float segs_w = static_cast<float>(theme::hud::segs * theme::hud::seg_w +
                                          (theme::hud::segs - 1) * theme::hud::seg_gap) *
                       scale;
  m.seg0 = {m.w - static_cast<float>(theme::space::lg) * scale - segs_w - 22.f * scale,
            m.h - 16.f * scale};
  m.conf = {m.w - static_cast<float>(theme::space::lg) * scale - 20.f * scale, m.h - 8.f * scale};
  m.chip = {0.f, m.rule_y + 3.f * scale, m.w, static_cast<float>(theme::hud::chip_h) * scale};
  return m;
}

}  // namespace airmouse
