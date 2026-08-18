#include "airmouse/camera/capture.hpp"

#ifndef _WIN32

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace airmouse {
namespace {

std::string read_sysfs(const std::string& path) {
  std::ifstream in(path);
  if (!in) return {};
  std::string line;
  std::getline(in, line);
  while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
    line.pop_back();
  }
  return line;
}

bool is_capture_fd(int fd, std::string* card_out) {
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) return false;
  if (card_out) {
    *card_out = reinterpret_cast<const char*>(cap.card);
  }
  const uint32_t caps = cap.device_caps ? cap.device_caps : cap.capabilities;
  return (caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) != 0;
}

bool is_laptop_chassis() {
  const std::string raw = read_sysfs("/sys/class/dmi/id/chassis_type");
  if (raw.empty()) return false;
  char* end = nullptr;
  const long type = std::strtol(raw.c_str(), &end, 10);
  if (end == raw.c_str()) return false;
  switch (type) {
    case 8:   // Portable
    case 9:   // Laptop
    case 10:  // Notebook
    case 11:  // Hand Held
    case 14:  // Sub Notebook
    case 30:  // Tablet
    case 31:  // Convertible
    case 32:  // Detachable
      return true;
    default:
      return false;
  }
}

bool usb_video_present() {
  DIR* dir = opendir("/sys/bus/usb/devices");
  if (!dir) return false;
  bool found = false;
  while (const dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.') continue;
    const std::string base = std::string("/sys/bus/usb/devices/") + ent->d_name;
    const std::string vendor = read_sysfs(base + "/idVendor");
    const std::string product = read_sysfs(base + "/idProduct");
    if (vendor == "30c9" && product == "0042") {
      found = true;
      break;
    }
    const std::string cls = read_sysfs(base + "/bInterfaceClass");
    if (cls == "0e" || cls == "0E" || cls == "0e\r") {
      found = true;
      break;
    }
  }
  closedir(dir);
  return found;
}

}  // namespace

std::vector<CameraDevice> list_camera_devices() {
  std::vector<CameraDevice> out;
  for (int i = 0; i < 64; ++i) {
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/video%d", i);
    const int fd = ::open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) continue;
    CameraDevice dev;
    dev.index = i;
    dev.path = path;
    const bool capture = is_capture_fd(fd, &dev.name);
    ::close(fd);
    if (!capture) continue;
    if (dev.name.empty()) {
      dev.name = path;
    } else if (const auto colon = dev.name.find(':'); colon != std::string::npos) {
      // V4L2 card is 32 bytes; "Integrated Camera: Integrated C" -> "Integrated Camera"
      dev.name.erase(colon);
    }
    dev.capture = true;
    out.push_back(std::move(dev));
  }
  return out;
}

std::vector<int> list_camera_indices() {
  std::vector<int> out;
  for (const auto& dev : list_camera_devices()) {
    out.push_back(dev.index);
  }
  return out;
}

CameraAbsence diagnose_camera() {
  if (!list_camera_indices().empty()) return CameraAbsence::Present;
  if (usb_video_present()) return CameraAbsence::Unsupported;
  if (is_laptop_chassis()) return CameraAbsence::PoweredOff;
  return CameraAbsence::NoDevice;
}

}  // namespace airmouse

#endif
