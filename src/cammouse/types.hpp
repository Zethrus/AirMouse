#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace cammouse {

inline constexpr int kLandmarkCount = 21;

enum LandmarkId : int {
  kWrist = 0,
  kThumbCmc = 1,
  kThumbMcp = 2,
  kThumbIp = 3,
  kThumbTip = 4,
  kIndexMcp = 5,
  kIndexPip = 6,
  kIndexDip = 7,
  kIndexTip = 8,
  kMiddleMcp = 9,
  kMiddlePip = 10,
  kMiddleDip = 11,
  kMiddleTip = 12,
  kRingMcp = 13,
  kRingPip = 14,
  kRingDip = 15,
  kRingTip = 16,
  kPinkyMcp = 17,
  kPinkyPip = 18,
  kPinkyDip = 19,
  kPinkyTip = 20,
};

struct Vec2 {
  float x = 0;
  float y = 0;
};

struct Landmark {
  float x = 0;
  float y = 0;
  float z = 0;
};

enum class HandSide { Left, Right, Unknown };

struct HandFrame {
  std::array<Landmark, kLandmarkCount> landmarks{};
  HandSide side = HandSide::Unknown;
  float presence = 1.f;
};

enum class PoseName {
  None,
  Point,
  Pinch,
  Drag,
  RightClick,
  Scroll,
  Clutch,
  Lock,
  Unlock,
  Lost,
};

inline constexpr std::string_view pose_label(PoseName pose) {
  switch (pose) {
    case PoseName::None:
      return "IDLE";
    case PoseName::Point:
      return "POINT";
    case PoseName::Pinch:
      return "PINCH";
    case PoseName::Drag:
      return "DRAG";
    case PoseName::RightClick:
      return "RIGHT";
    case PoseName::Scroll:
      return "SCROLL";
    case PoseName::Clutch:
      return "CLUTCH";
    case PoseName::Lock:
      return "LOCK";
    case PoseName::Unlock:
      return "UNLOCK";
    case PoseName::Lost:
      return "LOST";
  }
  return "IDLE";
}

enum class Button { Idle, Left, Right };

struct PointerCommand {
  bool has_target = false;
  float nx = 0;  // 0-1 image space, already mirrored if configured
  float ny = 0;
  Button press = Button::Idle;
  Button release = Button::Idle;
  int wheel = 0;
  PoseName pose = PoseName::None;
  bool release_all = false;
  float confidence = 0;
};

struct ScreenGeometry {
  int origin_x = 0;
  int origin_y = 0;
  int width = 1920;
  int height = 1080;
};

struct TrackingSnapshot {
  std::optional<HandFrame> hand;
  PointerCommand command;
  PoseName pose = PoseName::None;
  float fps = 0;
  bool camera_ok = false;
  std::string_view status;
};

}  // namespace cammouse
