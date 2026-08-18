#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cammouse {

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

}  // namespace cammouse
