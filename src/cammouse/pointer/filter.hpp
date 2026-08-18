#pragma once

#include "cammouse/types.hpp"

namespace cammouse {

class OneEuroFilter {
 public:
  OneEuroFilter(float mincutoff, float beta, float dcutoff);

  void set_params(float mincutoff, float beta, float dcutoff);
  void reset();
  Vec2 filter(Vec2 value, float dt_seconds);
  void tighten(float mincutoff);

 private:
  float lowpass(float prev, float value, float alpha) const;
  float alpha(float cutoff, float dt) const;

  float mincutoff_;
  float beta_;
  float dcutoff_;
  bool primed_ = false;
  Vec2 hat_x_{};
  Vec2 hat_dx_{};
};

}  // namespace cammouse
