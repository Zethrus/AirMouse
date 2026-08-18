#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "cammouse/camera/capture.hpp"
#include "cammouse/types.hpp"

namespace cammouse {

class HandLandmarker {
 public:
  virtual ~HandLandmarker() = default;
  virtual bool open(const std::filesystem::path& model) = 0;
  virtual std::vector<HandFrame> detect(const RgbFrame& frame, int64_t timestamp_ms) = 0;
  virtual std::string last_error() const = 0;
};

std::unique_ptr<HandLandmarker> create_mediapipe_landmarker();

}  // namespace cammouse
