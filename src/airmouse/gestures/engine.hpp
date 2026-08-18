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
};

}  // namespace airmouse
