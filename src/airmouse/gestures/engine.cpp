#include "airmouse/gestures/engine.hpp"

#include <algorithm>
#include <cmath>

namespace airmouse {
namespace {

constexpr int64_t kClickMaxMs = 280;
constexpr int64_t kClickMinMs = 50;
constexpr int64_t kRefractoryMs = 200;
constexpr int64_t kClutchMs = 250;
constexpr int64_t kLockMs = 700;
constexpr float kScrollTick = 0.035f;

}  // namespace

GestureEngine::GestureEngine(Config cfg) : cfg_(std::move(cfg)) {}

void GestureEngine::set_config(const Config& cfg) { cfg_ = cfg; }

const HandFrame* GestureEngine::select_hand(
    const std::vector<HandFrame>& /*unused*/, const HandFrame& hand) const {
  if (cfg_.dominant_hand == "left" && hand.side == HandSide::Right) {
    return nullptr;
  }
  if (cfg_.dominant_hand == "right" && hand.side == HandSide::Left) {
    return nullptr;
  }
  return &hand;
}

void GestureEngine::reset_transient() {
  left_pinched_ = false;
  dragging_ = false;
  right_pinched_ = false;
  scroll_primed_ = false;
  clutched_ = false;
  palm_hold_ms_ = -1;
  fist_hold_ms_ = -1;
  left_on_streak_ = 0;
  left_off_streak_ = 0;
  right_on_streak_ = 0;
  right_off_streak_ = 0;
  freeze_ = false;
  have_point_ = false;
  feat_primed_ = false;
  last_hand_.reset();
}

Vec2 GestureEngine::mirrored(const Vec2& p) const {
  return {cfg_.mirror ? 1.f - p.x : p.x, p.y};
}

Vec2 GestureEngine::pointing_cursor(const PoseFeatures& feat) const {
  return mirrored(feat.index_pip);
}

bool GestureEngine::debounced(bool raw_on, bool currently, int& on_streak, int& off_streak) const {
  const int need = std::max(1, cfg_.tracking.pinch_frames);
  if (raw_on) {
    ++on_streak;
    off_streak = 0;
  } else {
    ++off_streak;
    on_streak = 0;
  }
  if (!currently) return on_streak >= need;
  return off_streak < need;
}

PointerCommand GestureEngine::update(const std::optional<HandFrame>& hand_in, int64_t now_ms) {
  PointerCommand cmd;

  std::optional<HandFrame> held = hand_in;
  if (hand_in) {
    last_hand_ = *hand_in;
    last_seen_ms_ = now_ms;
  } else if (had_hand_ && last_hand_ && last_seen_ms_ >= 0 &&
             now_ms - last_seen_ms_ <= cfg_.tracking.lost_hold_ms) {
    held = last_hand_;
  }

  cmd.confidence = held ? held->presence : 0.f;

  if (!held) {
    if (had_hand_) {
      cmd.release_all = true;
      cmd.pose = PoseName::Lost;
      reset_transient();
    } else {
      cmd.pose = locked_ ? PoseName::Lock : PoseName::None;
    }
    had_hand_ = false;
    return cmd;
  }

  const HandFrame* chosen = select_hand({}, *held);
  if (!chosen) {
    cmd.pose = PoseName::None;
    return cmd;
  }

  had_hand_ = true;
  const PoseFeatures feat =
      feat_primed_ ? analyze_pose(*chosen, last_feat_) : analyze_pose(*chosen);
  last_feat_ = feat;
  feat_primed_ = true;

  const Vec2 pip = pointing_cursor(feat);

  auto emit = [&](float nx, float ny) {
    cmd.has_target = true;
    cmd.nx = nx;
    cmd.ny = ny;
  };

  const bool palm = feat.open_palm;
  const bool fist = feat.fist;

  if (palm) {
    if (palm_hold_ms_ < 0) {
      palm_hold_ms_ = now_ms;
    }
  } else {
    palm_hold_ms_ = -1;
  }
  if (fist) {
    if (fist_hold_ms_ < 0) {
      fist_hold_ms_ = now_ms;
    }
  } else {
    fist_hold_ms_ = -1;
  }

  if (locked_) {
    if (cfg_.gestures.lock && palm && palm_hold_ms_ >= 0 &&
        now_ms - palm_hold_ms_ >= kLockMs) {
      locked_ = false;
      cmd.pose = PoseName::Unlock;
      palm_hold_ms_ = now_ms;
    } else {
      cmd.pose = PoseName::Lock;
    }
    return cmd;
  }

  if (cfg_.gestures.lock && fist && fist_hold_ms_ >= 0 &&
      now_ms - fist_hold_ms_ >= kLockMs) {
    locked_ = true;
    reset_transient();
    had_hand_ = true;
    last_hand_ = *chosen;
    last_seen_ms_ = now_ms;
    feat_primed_ = true;
    last_feat_ = feat;
    cmd.release_all = true;
    cmd.pose = PoseName::Lock;
    return cmd;
  }

  if (cfg_.gestures.clutch && palm && palm_hold_ms_ >= 0 &&
      now_ms - palm_hold_ms_ >= kClutchMs) {
    clutched_ = true;
  } else if (clutched_ && !palm) {
    clutched_ = false;
  }

  if (clutched_) {
    cmd.pose = PoseName::Clutch;
    return cmd;
  }

  const bool left_raw = feat.pinch_index < (left_pinched_ ? cfg_.pinch_off : cfg_.pinch_on);
  const bool left_now = debounced(left_raw, left_pinched_, left_on_streak_, left_off_streak_);
  const bool right_raw =
      !left_now && feat.pinch_middle < (right_pinched_ ? cfg_.pinch_off : cfg_.pinch_on);
  const bool right_now =
      debounced(right_raw, right_pinched_, right_on_streak_, right_off_streak_);

  if (!freeze_ && !left_raw && !right_raw) {
    last_point_nx_ = pip.x;
    last_point_ny_ = pip.y;
    have_point_ = true;
  }

  if (cfg_.gestures.left_pinch && left_now) {
    if (!left_pinched_) {
      left_pinched_ = true;
      left_pinch_start_ms_ = now_ms;
      freeze_nx_ = have_point_ ? last_point_nx_ : pip.x;
      freeze_ny_ = have_point_ ? last_point_ny_ : pip.y;
      freeze_ = true;
    }
    const int64_t held_ms = now_ms - left_pinch_start_ms_;
    if (cfg_.gestures.drag && held_ms > kClickMaxMs) {
      if (!dragging_) {
        dragging_ = true;
        cmd.press = Button::Left;
        const Vec2 wrist = mirrored(feat.wrist);
        drag_wrist_x_ = wrist.x;
        drag_wrist_y_ = wrist.y;
        drag_origin_nx_ = freeze_nx_;
        drag_origin_ny_ = freeze_ny_;
      }
      const Vec2 wrist = mirrored(feat.wrist);
      emit(drag_origin_nx_ + (wrist.x - drag_wrist_x_),
           drag_origin_ny_ + (wrist.y - drag_wrist_y_));
      cmd.pose = PoseName::Drag;
    } else {
      emit(freeze_nx_, freeze_ny_);
      cmd.pose = PoseName::Pinch;
    }
    return cmd;
  }

  if (left_pinched_) {
    const int64_t held_ms = now_ms - left_pinch_start_ms_;
    if (dragging_) {
      cmd.release = Button::Left;
    } else if (cfg_.gestures.left_pinch && held_ms >= kClickMinMs &&
               now_ms - last_click_ms_ >= kRefractoryMs) {
      cmd.press = Button::Left;
      cmd.release = Button::Left;
      last_click_ms_ = now_ms;
    }
    dragging_ = false;
    left_pinched_ = false;
    freeze_ = false;
    emit(have_point_ ? last_point_nx_ : pip.x, have_point_ ? last_point_ny_ : pip.y);
    cmd.pose = PoseName::Point;
    return cmd;
  }

  if (cfg_.gestures.right_pinch && right_now) {
    if (!right_pinched_) {
      right_pinched_ = true;
      last_event_ms_ = now_ms;
      freeze_nx_ = have_point_ ? last_point_nx_ : pip.x;
      freeze_ny_ = have_point_ ? last_point_ny_ : pip.y;
      freeze_ = true;
    }
    emit(freeze_nx_, freeze_ny_);
    cmd.pose = PoseName::RightClick;
    return cmd;
  }

  if (right_pinched_) {
    const int64_t held_ms = now_ms - last_event_ms_;
    if (held_ms >= kClickMinMs && now_ms - last_click_ms_ >= kRefractoryMs) {
      cmd.press = Button::Right;
      cmd.release = Button::Right;
      last_click_ms_ = now_ms;
    }
    right_pinched_ = false;
    freeze_ = false;
    emit(have_point_ ? last_point_nx_ : pip.x, have_point_ ? last_point_ny_ : pip.y);
    cmd.pose = PoseName::Point;
    return cmd;
  }

  if (cfg_.gestures.scroll && feat.two_finger) {
    cmd.pose = PoseName::Scroll;
    if (!scroll_primed_) {
      last_scroll_y_ = feat.index_pip.y;
      scroll_primed_ = true;
    } else {
      const float dy = feat.index_pip.y - last_scroll_y_;
      if (std::fabs(dy) >= kScrollTick) {
        cmd.wheel = dy > 0 ? -1 : 1;
        last_scroll_y_ = feat.index_pip.y;
      }
    }
    emit(pip.x, pip.y);
    return cmd;
  }
  scroll_primed_ = false;

  if (have_point_ && (left_raw || right_raw)) {
    emit(last_point_nx_, last_point_ny_);
  } else {
    emit(pip.x, pip.y);
  }
  cmd.pose = feat.index_up ? PoseName::Point : PoseName::None;
  return cmd;
}

}  // namespace airmouse
