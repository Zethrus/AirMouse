#pragma once

#include <algorithm>
#include <cmath>

#include "airmouse/ui/theme.hpp"

namespace airmouse {

struct HudRect {
  float x = 0;
  float y = 0;
  float w = 0;
  float h = 0;
};

struct WorkArea {
  float x = 0;
  float y = 0;
  float w = 0;
  float h = 0;
};

struct HudVec {
  float x = 0;
  float y = 0;
};

inline float layout_pad(float scale) {
  return static_cast<float>(theme::hud::pad) * std::max(scale, 0.01f);
}

inline float layout_snap(float scale) {
  return theme::motion::snap_px * std::max(scale, 0.01f);
}

inline HudVec default_position(const WorkArea& work, float hud_w, float hud_h, float scale = 1.f) {
  (void)hud_h;
  const float pad = layout_pad(scale);
  return {work.x + work.w - hud_w - pad, work.y + pad};
}

inline HudVec clamp_position(const WorkArea& work, float x, float y, float hud_w, float hud_h) {
  const float max_x = work.x + std::max(0.f, work.w - hud_w);
  const float max_y = work.y + std::max(0.f, work.h - hud_h);
  return {std::clamp(x, work.x, max_x), std::clamp(y, work.y, max_y)};
}

inline HudVec snap_target(const WorkArea& work, float x, float y, float hud_w, float hud_h,
                          float scale = 1.f) {
  const float pad = layout_pad(scale);
  const float snap = layout_snap(scale);
  const float left = work.x + pad;
  const float top = work.y + pad;
  const float right = work.x + work.w - hud_w - pad;
  const float bottom = work.y + work.h - hud_h - pad;
  HudVec out{x, y};
  if (std::abs(x - left) <= snap) out.x = left;
  else if (std::abs(x - right) <= snap) out.x = right;
  if (std::abs(y - top) <= snap) out.y = top;
  else if (std::abs(y - bottom) <= snap) out.y = bottom;
  return clamp_position(work, out.x, out.y, hud_w, hud_h);
}

inline bool offscreen(const WorkArea& work, float x, float y, float hud_w, float hud_h) {
  return x + hud_w <= work.x || y + hud_h <= work.y || x >= work.x + work.w || y >= work.y + work.h;
}

inline HudRect grip_rect(float hud_w, float scale = 1.f) {
  return {0.f, 0.f, hud_w, static_cast<float>(theme::hud::grip_h) * scale};
}

inline bool in_grip(float local_x, float local_y, float hud_w, float scale = 1.f) {
  const HudRect g = grip_rect(hud_w, scale);
  return local_x >= g.x && local_y >= g.y && local_x < g.x + g.w && local_y < g.y + g.h;
}

inline float ease_exp(float current, float target, float dt, float tau) {
  if (tau <= 1e-6f) return target;
  const float t = 1.f - std::exp(-std::max(dt, 0.f) / tau);
  return current + (target - current) * t;
}

inline void spring_step(float& pos, float& vel, float target, float dt, float tau) {
  dt = std::max(dt, 0.f);
  const float omega = 1.f / std::max(tau, 1e-4f);
  const float acc = -2.f * omega * vel - omega * omega * (pos - target);
  vel += acc * dt;
  pos += vel * dt;
}

inline bool spring_settled(float pos, float vel, float target) {
  return std::abs(pos - target) < theme::motion::settle_pos &&
         std::abs(vel) < theme::motion::settle_vel;
}

}  // namespace airmouse
