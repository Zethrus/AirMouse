#include "airmouse/tracking/landmarker.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "mediapipe/tasks/c/core/base_options.h"
#include "mediapipe/tasks/c/core/mp_status.h"
#include "mediapipe/tasks/c/vision/core/image.h"
#include "mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker.h"
#include "mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker_result.h"

namespace airmouse {
namespace {

HandSide parse_side(const Categories* handedness) {
  if (!handedness || handedness->categories_count == 0 || !handedness->categories) {
    return HandSide::Unknown;
  }
  const char* name = handedness->categories[0].category_name;
  if (!name) return HandSide::Unknown;
  if (std::strcmp(name, "Left") == 0) return HandSide::Left;
  if (std::strcmp(name, "Right") == 0) return HandSide::Right;
  return HandSide::Unknown;
}

class MediaPipeLandmarker final : public HandLandmarker {
 public:
  ~MediaPipeLandmarker() override {
    if (ptr_) {
      char* err = nullptr;
      MpHandLandmarkerClose(ptr_, &err);
      if (err) free(err);
    }
  }

  bool open(const std::filesystem::path& model, LandmarkerOptions thresholds = {}) override {
    if (ptr_) {
      char* close_err = nullptr;
      MpHandLandmarkerClose(ptr_, &close_err);
      if (close_err) free(close_err);
      ptr_ = nullptr;
    }
    last_ts_ = -1;
    model_path_ = model.string();
    HandLandmarkerOptions opts{};
    opts.base_options.model_asset_path = model_path_.c_str();
    opts.base_options.delegate = CPU;
    opts.base_options.host_environment = HOST_ENVIRONMENT_UNKNOWN;
    opts.base_options.host_system =
#ifdef _WIN32
        HOST_SYSTEM_WINDOWS;
#else
        HOST_SYSTEM_LINUX;
#endif
    opts.running_mode = VIDEO;
    opts.num_hands = 2;
    opts.min_hand_detection_confidence = thresholds.min_detection;
    opts.min_hand_presence_confidence = thresholds.min_presence;
    opts.min_tracking_confidence = thresholds.min_tracking;
    opts.result_callback = nullptr;

    char* err = nullptr;
    const MpStatus st = MpHandLandmarkerCreate(&opts, &ptr_, &err);
    if (st != kMpOk || !ptr_) {
      err_ = err ? err : "MpHandLandmarkerCreate failed";
      if (err) free(err);
      ptr_ = nullptr;
      return false;
    }
    err_.clear();
    return true;
  }

  std::vector<HandFrame> detect(const RgbFrame& frame, int64_t timestamp_ms) override {
    std::vector<HandFrame> hands;
    if (!ptr_ || frame.rgb.empty() || frame.width <= 0 || frame.height <= 0) {
      return hands;
    }
    MpImagePtr image = nullptr;
    char* err = nullptr;
    const int bytes = static_cast<int>(frame.rgb.size());
    MpStatus st = MpImageCreateFromUint8Data(kMpImageFormatSrgb, frame.width, frame.height,
                                             frame.rgb.data(), bytes, &image, &err);
    if (st != kMpOk || !image) {
      err_ = err ? err : "MpImageCreateFromUint8Data failed";
      if (err) free(err);
      return hands;
    }

    int64_t ts = timestamp_ms;
    if (ts <= last_ts_) ts = last_ts_ + 1;
    last_ts_ = ts;

    HandLandmarkerResult result{};
    st = MpHandLandmarkerDetectForVideo(ptr_, image, nullptr, ts, &result, &err);
    if (st != kMpOk) {
      err_ = err ? err : "MpHandLandmarkerDetectForVideo failed";
      if (err) free(err);
      MpImageFree(image);
      return hands;
    }

    const uint32_t n = result.hand_landmarks_count;
    hands.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
      const NormalizedLandmarks& lm = result.hand_landmarks[i];
      HandFrame hand;
      const uint32_t count = std::min<uint32_t>(lm.landmarks_count, kLandmarkCount);
      float presence_sum = 0.f;
      int presence_n = 0;
      for (uint32_t k = 0; k < count; ++k) {
        hand.landmarks[k].x = lm.landmarks[k].x;
        hand.landmarks[k].y = lm.landmarks[k].y;
        hand.landmarks[k].z = lm.landmarks[k].z;
        if (lm.landmarks[k].has_presence) {
          presence_sum += lm.landmarks[k].presence;
          ++presence_n;
        }
      }
      if (presence_n > 0) {
        hand.presence = presence_sum / static_cast<float>(presence_n);
      } else if (i < result.handedness_count && result.handedness[i].categories_count > 0 &&
                 result.handedness[i].categories) {
        hand.presence = result.handedness[i].categories[0].score;
      }
      if (i < result.handedness_count) {
        hand.side = parse_side(&result.handedness[i]);
      }
      hands.push_back(hand);
    }
    MpHandLandmarkerCloseResult(&result);
    MpImageFree(image);
    return hands;
  }

  std::string last_error() const override { return err_; }

 private:
  MpHandLandmarkerPtr ptr_ = nullptr;
  std::string model_path_;
  std::string err_;
  int64_t last_ts_ = -1;
};

}  // namespace

std::unique_ptr<HandLandmarker> create_mediapipe_landmarker() {
  return std::make_unique<MediaPipeLandmarker>();
}

}  // namespace airmouse
