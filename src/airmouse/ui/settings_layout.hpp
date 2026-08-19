#pragma once

#include <array>

#include "airmouse/ui/theme.hpp"

namespace airmouse {

struct SettingsHit {
  const char* id;
  int x;
  int y;
  int w;
  int h;
};

inline std::array<SettingsHit, 8> settings_hits() {
  using namespace theme;
  const int pad = space::xl;
  const int w = settings::w;
  const int h = settings::h;
  return {{
      {"cam", pad, 86, w - pad * 2, settings::cam_h},
      {"hud", pad, 136, 160, settings::row_h},
      {"chips", pad, 168, 160, settings::row_h},
      {"mirror", pad, 200, 160, settings::row_h},
      {"reset", pad, 232, 180, settings::row_h},
      {"box", pad, 284, w - pad * 2, settings::slider_h},
      {"open", pad, h - 64, 140, settings::action_h},
      {"done", w - 116, h - 64, 92, settings::action_h},
  }};
}

inline bool hit_contains(const SettingsHit& hit, int x, int y) {
  return x >= hit.x && y >= hit.y && x <= hit.x + hit.w && y <= hit.y + hit.h;
}

}  // namespace airmouse
