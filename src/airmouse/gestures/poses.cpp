#include "airmouse/gestures/poses.hpp"

#include <cmath>

namespace airmouse {

float distance(const Landmark& a, const Landmark& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float distance2(const Landmark& a, const Landmark& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

namespace {

constexpr float kIndexOnCos = -0.88f;
constexpr float kIndexOffCos = -0.55f;
constexpr float kOuterOnCos = -0.92f;
constexpr float kOuterOffCos = -0.65f;
constexpr float kThumbPalm = 0.45f;

float pip_cos(const HandFrame& hand, int mcp, int pip, int tip) {
  const Landmark& p = hand.landmarks[static_cast<size_t>(pip)];
  const Landmark& m = hand.landmarks[static_cast<size_t>(mcp)];
  const Landmark& t = hand.landmarks[static_cast<size_t>(tip)];
  const float v1x = m.x - p.x;
  const float v1y = m.y - p.y;
  const float v2x = t.x - p.x;
  const float v2y = t.y - p.y;
  const float n1 = std::sqrt(v1x * v1x + v1y * v1y);
  const float n2 = std::sqrt(v2x * v2x + v2y * v2y);
  if (n1 < 1e-6f || n2 < 1e-6f) return 1.f;
  return (v1x * v2x + v1y * v2y) / (n1 * n2);
}

bool finger_up(float cos_val, bool prev, float on_cos, float off_cos) {
  if (prev) return cos_val <= off_cos;
  return cos_val < on_cos;
}

}  // namespace

PoseFeatures analyze_pose(const HandFrame& hand) {
  return analyze_pose(hand, PoseFeatures{});
}

PoseFeatures analyze_pose(const HandFrame& hand, const PoseFeatures& prev) {
  PoseFeatures f;
  f.palm = distance2(hand.landmarks[kWrist], hand.landmarks[kMiddleMcp]);
  if (f.palm < 1e-4f) {
    f.palm = 1e-4f;
  }
  f.pinch_index = distance2(hand.landmarks[kThumbTip], hand.landmarks[kIndexTip]) / f.palm;
  f.pinch_middle = distance2(hand.landmarks[kThumbTip], hand.landmarks[kMiddleTip]) / f.palm;
  f.index_up = finger_up(pip_cos(hand, kIndexMcp, kIndexPip, kIndexTip), prev.index_up,
                         kIndexOnCos, kIndexOffCos);
  f.middle_up = finger_up(pip_cos(hand, kMiddleMcp, kMiddlePip, kMiddleTip), prev.middle_up,
                          kIndexOnCos, kIndexOffCos);
  f.ring_up = finger_up(pip_cos(hand, kRingMcp, kRingPip, kRingTip), prev.ring_up, kOuterOnCos,
                        kOuterOffCos);
  f.pinky_up = finger_up(pip_cos(hand, kPinkyMcp, kPinkyPip, kPinkyTip), prev.pinky_up,
                         kOuterOnCos, kOuterOffCos);
  f.thumb_up = distance2(hand.landmarks[kThumbTip], hand.landmarks[kIndexMcp]) > f.palm * kThumbPalm;
  f.fist = !f.index_up && !f.middle_up && !f.ring_up && !f.pinky_up;
  f.open_palm = f.index_up && f.middle_up && f.ring_up && f.pinky_up;
  f.two_finger = f.index_up && f.middle_up && !f.ring_up && !f.pinky_up && !f.thumb_up;
  f.index_tip = {hand.landmarks[kIndexTip].x, hand.landmarks[kIndexTip].y};
  f.index_pip = {hand.landmarks[kIndexPip].x, hand.landmarks[kIndexPip].y};
  f.index_mcp = {hand.landmarks[kIndexMcp].x, hand.landmarks[kIndexMcp].y};
  f.wrist = {hand.landmarks[kWrist].x, hand.landmarks[kWrist].y};
  return f;
}

}  // namespace airmouse
