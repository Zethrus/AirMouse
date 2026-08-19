#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "airmouse/camera/capture.hpp"
#include "airmouse/types.hpp"

namespace airmouse {

struct LandmarkerOptions {
  float min_detection = 0.35f;
  float min_presence = 0.40f;
  float min_tracking = 0.30f;
};

class HandLandmarker {
 public:
  virtual ~HandLandmarker() = default;
  virtual bool open(const std::filesystem::path& model, LandmarkerOptions opts = {}) = 0;
  virtual std::vector<HandFrame> detect(const RgbFrame& frame, int64_t timestamp_ms) = 0;
  virtual std::string last_error() const = 0;
};

std::unique_ptr<HandLandmarker> create_mediapipe_landmarker();

}  // namespace airmouse
