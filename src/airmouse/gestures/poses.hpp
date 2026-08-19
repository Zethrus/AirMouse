#pragma once

#include "airmouse/types.hpp"

namespace airmouse {

struct PoseFeatures {
  float palm = 1.f;
  float pinch_index = 10.f;
  float pinch_middle = 10.f;
  bool index_up = false;
  bool middle_up = false;
  bool ring_up = false;
  bool pinky_up = false;
  bool thumb_up = false;
  bool fist = false;
  bool open_palm = false;
  bool two_finger = false;
  Vec2 index_tip{};
  Vec2 index_pip{};
  Vec2 index_mcp{};
  Vec2 wrist{};
};

float distance(const Landmark& a, const Landmark& b);
float distance2(const Landmark& a, const Landmark& b);
PoseFeatures analyze_pose(const HandFrame& hand);
PoseFeatures analyze_pose(const HandFrame& hand, const PoseFeatures& prev);

}  // namespace airmouse
