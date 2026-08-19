#pragma once

#include "airmouse/types.hpp"

namespace airmouse {

class OneEuroFilter {
 public:
  OneEuroFilter() : OneEuroFilter(1.0f, 0.007f, 1.0f) {}
  OneEuroFilter(float mincutoff, float beta, float dcutoff);

  void set_params(float mincutoff, float beta, float dcutoff);
  void set_deadzone(float deadzone);
  void reset();
  Vec2 filter(Vec2 value, float dt_seconds);
  void tighten(float mincutoff);

 private:
  float lowpass(float prev, float value, float alpha) const;
  float alpha(float cutoff, float dt) const;

  float mincutoff_;
  float beta_;
  float dcutoff_;
  float deadzone_ = 0.f;
  bool primed_ = false;
  Vec2 hat_x_{};
  Vec2 hat_dx_{};
};

}  // namespace airmouse
