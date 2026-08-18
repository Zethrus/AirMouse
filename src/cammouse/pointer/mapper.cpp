#include "cammouse/pointer/mapper.hpp"

#include <algorithm>

namespace cammouse {

Vec2 Mapper::map(float nx, float ny, const ScreenGeometry& screen) const {
  const float box = std::clamp(control_box, 0.2f, 1.0f);
  const float margin = (1.f - box) * 0.5f;
  float u = (nx - margin) / box;
  float v = (ny - margin) / box;
  u = std::clamp(u, 0.f, 1.f);
  v = std::clamp(v, 0.f, 1.f);
  return {static_cast<float>(screen.origin_x) + u * static_cast<float>(screen.width),
          static_cast<float>(screen.origin_y) + v * static_cast<float>(screen.height)};
}

}  // namespace cammouse
