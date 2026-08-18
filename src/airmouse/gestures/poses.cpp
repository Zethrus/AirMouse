#include "airmouse/gestures/poses.hpp"

#include <cmath>

namespace airmouse {

float distance(const Landmark& a, const Landmark& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

namespace {

bool finger_extended(const HandFrame& hand, int tip, int pip) {
  const float tip_d = distance(hand.landmarks[static_cast<size_t>(tip)], hand.landmarks[kWrist]);
  const float pip_d = distance(hand.landmarks[static_cast<size_t>(pip)], hand.landmarks[kWrist]);
  return tip_d > pip_d * 1.08f;
}

}  // namespace

PoseFeatures analyze_pose(const HandFrame& hand) {
  PoseFeatures f;
  f.palm = distance(hand.landmarks[kWrist], hand.landmarks[kMiddleMcp]);
  if (f.palm < 1e-4f) {
    f.palm = 1e-4f;
  }
  f.pinch_index =
      distance(hand.landmarks[kThumbTip], hand.landmarks[kIndexTip]) / f.palm;
  f.pinch_middle =
      distance(hand.landmarks[kThumbTip], hand.landmarks[kMiddleTip]) / f.palm;
  f.index_up = finger_extended(hand, kIndexTip, kIndexPip);
  f.middle_up = finger_extended(hand, kMiddleTip, kMiddlePip);
  f.ring_up = finger_extended(hand, kRingTip, kRingPip);
  f.pinky_up = finger_extended(hand, kPinkyTip, kPinkyPip);
  f.thumb_up = distance(hand.landmarks[kThumbTip], hand.landmarks[kIndexMcp]) > f.palm * 0.45f;
  f.fist = !f.index_up && !f.middle_up && !f.ring_up && !f.pinky_up;
  f.open_palm = f.index_up && f.middle_up && f.ring_up && f.pinky_up;
  f.two_finger = f.index_up && f.middle_up && !f.ring_up && !f.pinky_up && !f.thumb_up;
  f.index_tip = {hand.landmarks[kIndexTip].x, hand.landmarks[kIndexTip].y};
  return f;
}

}  // namespace airmouse
