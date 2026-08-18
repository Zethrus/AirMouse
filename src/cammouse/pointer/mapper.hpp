#pragma once

#include "cammouse/types.hpp"

namespace cammouse {

struct Mapper {
  float control_box = 0.62f;

  Vec2 map(float nx, float ny, const ScreenGeometry& screen) const;
};

}  // namespace cammouse
