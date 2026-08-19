#pragma once

#include <array>

#include "airmouse/pointer/filter.hpp"
#include "airmouse/types.hpp"

namespace airmouse {

class LandmarkFilter {
 public:
  LandmarkFilter() : LandmarkFilter(1.2f, 0.007f, 1.0f) {}
  LandmarkFilter(float mincutoff, float beta, float dcutoff) {
    set_params(mincutoff, beta, dcutoff);
  }

  void set_params(float mincutoff, float beta, float dcutoff) {
    for (auto& f : filters_) {
      f.set_params(mincutoff, beta, dcutoff);
    }
  }

  void reset() {
    for (auto& f : filters_) {
      f.reset();
    }
  }

  HandFrame filter(const HandFrame& in, float dt) {
    HandFrame out = in;
    for (int i = 0; i < kLandmarkCount; ++i) {
      const Vec2 s = filters_[static_cast<size_t>(i)].filter(
          {in.landmarks[static_cast<size_t>(i)].x, in.landmarks[static_cast<size_t>(i)].y}, dt);
      out.landmarks[static_cast<size_t>(i)].x = s.x;
      out.landmarks[static_cast<size_t>(i)].y = s.y;
    }
    return out;
  }

 private:
  std::array<OneEuroFilter, kLandmarkCount> filters_{};
};

}  // namespace airmouse
