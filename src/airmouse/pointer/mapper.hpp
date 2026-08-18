#pragma once

#include "airmouse/types.hpp"

namespace airmouse {

struct Mapper {
  float control_box = 0.62f;

  Vec2 map(float nx, float ny, const ScreenGeometry& screen) const;
};

}  // namespace airmouse
