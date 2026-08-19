#pragma once

#include <cmath>

#include "airmouse/config.hpp"
#include "airmouse/ui/hud_layout.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {

struct HudDrag {
  enum class Phase { Idle, Hover, Dragging, Settling };

  Phase phase = Phase::Idle;
  float x = 0;
  float y = 0;
  float vx = 0;
  float vy = 0;
  float hover_t = 0;
  float lift_t = 0;
  float hud_w = static_cast<float>(theme::hud::w);
  float hud_h = static_cast<float>(theme::hud::h);
  float scale = 1.f;
  bool placed = false;

  void set_metrics(float w, float h, float s) {
    hud_w = w;
    hud_h = h;
    scale = std::max(s, 0.01f);
  }

  void place(const HudConfig& cfg, const WorkArea& work) {
    if (phase == Phase::Dragging) return;
    if (!cfg.placed || offscreen(work, static_cast<float>(cfg.x), static_cast<float>(cfg.y), hud_w,
                                 hud_h)) {
      const HudVec p = default_position(work, hud_w, hud_h, scale);
      x = p.x;
      y = p.y;
      placed = false;
    } else {
      const HudVec p = clamp_position(work, static_cast<float>(cfg.x), static_cast<float>(cfg.y),
                                      hud_w, hud_h);
      x = p.x;
      y = p.y;
      placed = true;
    }
    vx = 0;
    vy = 0;
    if (phase == Phase::Settling) phase = Phase::Idle;
  }

  HudConfig config() const {
    HudConfig cfg;
    cfg.placed = placed;
    cfg.x = static_cast<int>(std::lround(x));
    cfg.y = static_cast<int>(std::lround(y));
    return cfg;
  }

  void on_enter() {
    if (phase == Phase::Dragging || phase == Phase::Settling) return;
    phase = Phase::Hover;
  }

  void on_leave() {
    if (phase == Phase::Dragging || phase == Phase::Settling) return;
    phase = Phase::Idle;
  }

  bool on_press(float pointer_x, float pointer_y) {
    if (!in_grip(pointer_x - x, pointer_y - y, hud_w, scale)) return false;
    grab_dx_ = pointer_x - x;
    grab_dy_ = pointer_y - y;
    phase = Phase::Dragging;
    vx = 0;
    vy = 0;
    return true;
  }

  void on_move(float pointer_x, float pointer_y, const WorkArea& work) {
    if (phase != Phase::Dragging) return;
    const HudVec p = clamp_position(work, pointer_x - grab_dx_, pointer_y - grab_dy_, hud_w, hud_h);
    x = p.x;
    y = p.y;
  }

  bool on_release(const WorkArea& work) {
    if (phase != Phase::Dragging) return false;
    placed = true;
    const HudVec snap = snap_target(work, x, y, hud_w, hud_h, scale);
    if (std::abs(snap.x - x) > 0.5f || std::abs(snap.y - y) > 0.5f) {
      target_x_ = snap.x;
      target_y_ = snap.y;
      vx = 0;
      vy = 0;
      phase = Phase::Settling;
    } else {
      phase = Phase::Hover;
    }
    return true;
  }

  bool tick(float dt, const WorkArea& work) {
    const float hover_target =
        (phase == Phase::Hover || phase == Phase::Dragging || phase == Phase::Settling) ? 1.f : 0.f;
    const float lift_target = phase == Phase::Dragging ? 1.f : 0.f;
    const float next_hover = ease_exp(hover_t, hover_target, dt, theme::motion::hover_tau);
    const float next_lift = ease_exp(lift_t, lift_target, dt, theme::motion::lift_tau);
    bool dirty = std::abs(next_hover - hover_t) > 0.001f || std::abs(next_lift - lift_t) > 0.001f;
    hover_t = next_hover;
    lift_t = next_lift;

    if (phase == Phase::Settling) {
      const float px = x;
      const float py = y;
      spring_step(x, vx, target_x_, dt, theme::motion::spring_tau);
      spring_step(y, vy, target_y_, dt, theme::motion::spring_tau);
      const HudVec c = clamp_position(work, x, y, hud_w, hud_h);
      x = c.x;
      y = c.y;
      dirty = dirty || std::abs(x - px) > 0.01f || std::abs(y - py) > 0.01f;
      if (spring_settled(x, vx, target_x_) && spring_settled(y, vy, target_y_)) {
        x = target_x_;
        y = target_y_;
        vx = 0;
        vy = 0;
        phase = Phase::Hover;
        dirty = true;
      }
    }
    return dirty;
  }

  bool animating() const {
    return phase == Phase::Dragging || phase == Phase::Settling || hover_t > 0.001f ||
           lift_t > 0.001f;
  }

 private:
  float grab_dx_ = 0;
  float grab_dy_ = 0;
  float target_x_ = 0;
  float target_y_ = 0;
};

}  // namespace airmouse
