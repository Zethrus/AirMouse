#pragma once

#include <optional>
#include <vector>

#include "airmouse/config.hpp"
#include "airmouse/gestures/poses.hpp"
#include "airmouse/types.hpp"

namespace airmouse {

class GestureEngine {
 public:
  explicit GestureEngine(Config cfg);

  void set_config(const Config& cfg);
  PointerCommand update(const std::optional<HandFrame>& hand, int64_t now_ms);
  bool locked() const { return locked_; }
  bool clutched() const { return clutched_; }

 private:
  const HandFrame* select_hand(const std::vector<HandFrame>& /*unused*/,
                               const HandFrame& hand) const;
  void reset_transient();
  Vec2 mirrored(const Vec2& p) const;
  Vec2 pointing_cursor(const PoseFeatures& feat) const;
  bool debounced(bool raw_on, bool currently, int& on_streak, int& off_streak) const;

  Config cfg_;
  bool had_hand_ = false;
  bool locked_ = false;
  bool clutched_ = false;
  bool left_pinched_ = false;
  bool dragging_ = false;
  bool right_pinched_ = false;
  int64_t left_pinch_start_ms_ = -1;
  int64_t palm_hold_ms_ = -1;
  int64_t fist_hold_ms_ = -1;
  int64_t last_event_ms_ = 0;
  int64_t last_click_ms_ = -10000;
  float last_scroll_y_ = 0;
  bool scroll_primed_ = false;

  std::optional<HandFrame> last_hand_;
  int64_t last_seen_ms_ = -1;
  PoseFeatures last_feat_{};
  bool feat_primed_ = false;
  int left_on_streak_ = 0;
  int left_off_streak_ = 0;
  int right_on_streak_ = 0;
  int right_off_streak_ = 0;
  bool have_point_ = false;
  float last_point_nx_ = 0;
  float last_point_ny_ = 0;
  bool freeze_ = false;
  float freeze_nx_ = 0;
  float freeze_ny_ = 0;
  float drag_wrist_x_ = 0;
  float drag_wrist_y_ = 0;
  float drag_origin_nx_ = 0;
  float drag_origin_ny_ = 0;
};

}  // namespace airmouse
