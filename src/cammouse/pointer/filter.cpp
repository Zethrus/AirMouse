#include "cammouse/pointer/filter.hpp"

#include <algorithm>
#include <cmath>

namespace cammouse {

OneEuroFilter::OneEuroFilter(float mincutoff, float beta, float dcutoff)
    : mincutoff_(mincutoff), beta_(beta), dcutoff_(dcutoff) {}

void OneEuroFilter::set_params(float mincutoff, float beta, float dcutoff) {
  mincutoff_ = mincutoff;
  beta_ = beta;
  dcutoff_ = dcutoff;
}

void OneEuroFilter::reset() { primed_ = false; }

void OneEuroFilter::tighten(float mincutoff) { mincutoff_ = mincutoff; }

float OneEuroFilter::alpha(float cutoff, float dt) const {
  const float tau = 1.f / (2.f * 3.14159265f * std::max(cutoff, 1e-6f));
  return 1.f / (1.f + tau / std::max(dt, 1e-6f));
}

float OneEuroFilter::lowpass(float prev, float value, float a) const {
  return a * value + (1.f - a) * prev;
}

Vec2 OneEuroFilter::filter(Vec2 value, float dt_seconds) {
  if (!primed_) {
    primed_ = true;
    hat_x_ = value;
    hat_dx_ = {};
    return value;
  }
  const float inv_dt = 1.f / std::max(dt_seconds, 1e-6f);
  const Vec2 vel{(value.x - hat_x_.x) * inv_dt, (value.y - hat_x_.y) * inv_dt};
  const float ad = alpha(dcutoff_, dt_seconds);
  hat_dx_.x = lowpass(hat_dx_.x, vel.x, ad);
  hat_dx_.y = lowpass(hat_dx_.y, vel.y, ad);
  const float cutoff_x = mincutoff_ + beta_ * std::fabs(hat_dx_.x);
  const float cutoff_y = mincutoff_ + beta_ * std::fabs(hat_dx_.y);
  hat_x_.x = lowpass(hat_x_.x, value.x, alpha(cutoff_x, dt_seconds));
  hat_x_.y = lowpass(hat_x_.y, value.y, alpha(cutoff_y, dt_seconds));
  return hat_x_;
}

}  // namespace cammouse
