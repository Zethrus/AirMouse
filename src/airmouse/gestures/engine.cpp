#include "airmouse/gestures/engine.hpp"

#include <algorithm>
#include <cmath>

namespace airmouse {
namespace {

constexpr int64_t kClickMaxMs = 180;
constexpr int64_t kClickMinMs = 80;
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

PointerCommand GestureEngine::update(const std::optional<HandFrame>& hand,
                                     int64_t now_ms) {
  PointerCommand cmd;
  cmd.confidence = hand ? hand->presence : 0.f;

  if (!hand) {
    if (had_hand_) {
      cmd.release_all = true;
      cmd.pose = PoseName::Lost;
      left_pinched_ = false;
      dragging_ = false;
      right_pinched_ = false;
      scroll_primed_ = false;
      clutched_ = false;
      palm_hold_ms_ = -1;
      fist_hold_ms_ = -1;
    } else {
      cmd.pose = locked_ ? PoseName::Lock : PoseName::None;
    }
    had_hand_ = false;
    return cmd;
  }

  const HandFrame* chosen = select_hand({}, *hand);
  if (!chosen) {
    cmd.pose = PoseName::None;
    return cmd;
  }

  had_hand_ = true;
  const PoseFeatures feat = analyze_pose(*chosen);
  float tip_x = feat.index_tip.x;
  if (cfg_.mirror) {
    tip_x = 1.f - tip_x;
  }

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
    clutched_ = false;
    left_pinched_ = false;
    dragging_ = false;
    right_pinched_ = false;
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

  const bool left_now = feat.pinch_index < (left_pinched_ ? cfg_.pinch_off : cfg_.pinch_on);
  const bool right_now =
      !left_now && feat.pinch_middle < (right_pinched_ ? cfg_.pinch_off : cfg_.pinch_on);

  if (cfg_.gestures.left_pinch && left_now) {
    if (!left_pinched_) {
      left_pinched_ = true;
      left_pinch_start_ms_ = now_ms;
    }
    const int64_t held = now_ms - left_pinch_start_ms_;
    if (cfg_.gestures.drag && held > kClickMaxMs) {
      if (!dragging_) {
        dragging_ = true;
        cmd.press = Button::Left;
      }
      cmd.pose = PoseName::Drag;
    } else {
      cmd.pose = PoseName::Pinch;
    }
    cmd.has_target = true;
    cmd.nx = tip_x;
    cmd.ny = feat.index_tip.y;
    return cmd;
  }

  if (left_pinched_) {
    const int64_t held = now_ms - left_pinch_start_ms_;
    if (dragging_) {
      cmd.release = Button::Left;
    } else if (cfg_.gestures.left_pinch && held >= kClickMinMs &&
               now_ms - last_click_ms_ >= kRefractoryMs) {
      cmd.press = Button::Left;
      cmd.release = Button::Left;
      last_click_ms_ = now_ms;
    }
    dragging_ = false;
    left_pinched_ = false;
    cmd.has_target = true;
    cmd.nx = tip_x;
    cmd.ny = feat.index_tip.y;
    cmd.pose = PoseName::Point;
    return cmd;
  }

  if (cfg_.gestures.right_pinch && right_now) {
    if (!right_pinched_) {
      right_pinched_ = true;
      last_event_ms_ = now_ms;
    }
    cmd.pose = PoseName::RightClick;
    cmd.has_target = true;
    cmd.nx = tip_x;
    cmd.ny = feat.index_tip.y;
    return cmd;
  }

  if (right_pinched_) {
    const int64_t held = now_ms - last_event_ms_;
    if (held >= kClickMinMs && now_ms - last_click_ms_ >= kRefractoryMs) {
      cmd.press = Button::Right;
      cmd.release = Button::Right;
      last_click_ms_ = now_ms;
    }
    right_pinched_ = false;
    cmd.has_target = true;
    cmd.nx = tip_x;
    cmd.ny = feat.index_tip.y;
    cmd.pose = PoseName::Point;
    return cmd;
  }

  if (cfg_.gestures.scroll && feat.two_finger) {
    cmd.pose = PoseName::Scroll;
    if (!scroll_primed_) {
      last_scroll_y_ = feat.index_tip.y;
      scroll_primed_ = true;
    } else {
      const float dy = feat.index_tip.y - last_scroll_y_;
      if (std::fabs(dy) >= kScrollTick) {
        cmd.wheel = dy > 0 ? -1 : 1;
        last_scroll_y_ = feat.index_tip.y;
      }
    }
    cmd.has_target = true;
    cmd.nx = tip_x;
    cmd.ny = feat.index_tip.y;
    return cmd;
  }
  scroll_primed_ = false;

  cmd.has_target = true;
  cmd.nx = tip_x;
  cmd.ny = feat.index_tip.y;
  cmd.pose = feat.index_up ? PoseName::Point : PoseName::None;
  return cmd;
}

}  // namespace airmouse
