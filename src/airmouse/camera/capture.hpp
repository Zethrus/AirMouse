#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace airmouse {

struct RgbFrame {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgb;  // packed RGB8
};

class Camera {
 public:
  virtual ~Camera() = default;
  virtual bool open(int index, int width, int height, int fps) = 0;
  virtual void close() = 0;
  virtual bool read(RgbFrame& out) = 0;
  virtual bool ok() const = 0;
  virtual std::string last_error() const = 0;
};

std::unique_ptr<Camera> create_camera();
std::vector<int> list_camera_indices();

enum class CameraAbsence {
  Present,      // at least one capture node
  NoDevice,     // no webcam on this machine
  PoweredOff,   // laptop camera gated by a hardware/Fn key
  Permission,   // node exists, EACCES
  Busy,         // EBUSY
  Unsupported,  // USB camera present, no usable V4L2 capture
};

struct CameraDevice {
  int index = -1;
  std::string path;
  std::string name;
  bool capture = false;
};

std::vector<CameraDevice> list_camera_devices();
CameraAbsence diagnose_camera();

inline const char* camera_absence_status(CameraAbsence absence) {
  return absence == CameraAbsence::PoweredOff ? "CAM OFF" : "NO CAM";
}

inline std::string camera_absence_message(CameraAbsence absence) {
  switch (absence) {
    case CameraAbsence::Present:
      return {};
    case CameraAbsence::PoweredOff:
      return "Press the camera key, then wait";
    case CameraAbsence::Permission:
      return "Add user to video group, log out";
    case CameraAbsence::Busy:
      return "Camera in use by another app";
    case CameraAbsence::NoDevice:
      return "No webcam found";
    case CameraAbsence::Unsupported:
      return "Camera found, cannot capture";
  }
  return "No webcam found";
}

}  // namespace airmouse
