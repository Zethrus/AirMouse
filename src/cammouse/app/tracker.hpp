#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "cammouse/camera/capture.hpp"
#include "cammouse/config.hpp"
#include "cammouse/gestures/engine.hpp"
#include "cammouse/input/backend.hpp"
#include "cammouse/pointer/filter.hpp"
#include "cammouse/pointer/mapper.hpp"
#include "cammouse/tracking/landmarker.hpp"
#include "cammouse/types.hpp"

namespace cammouse {

class Tracker {
 public:
  explicit Tracker(Config cfg);
  ~Tracker();

  bool start();
  void stop();
  void set_paused(bool paused);
  bool paused() const { return paused_.load(); }
  void toggle_pause() { set_paused(!paused()); }
  void set_config(const Config& cfg);
  TrackingSnapshot snapshot() const;
  std::string last_error() const;

 private:
  void loop();
  void apply(const PointerCommand& cmd, float dt);

  Config cfg_;
  GestureEngine engine_;
  Mapper mapper_;
  OneEuroFilter filter_;
  ScreenGeometry screen_{};
  std::unique_ptr<Camera> camera_;
  std::unique_ptr<HandLandmarker> landmarker_;
  std::unique_ptr<InputBackend> input_;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};
  mutable std::mutex mu_;
  TrackingSnapshot snap_{};
  std::string error_;
  int64_t last_ms_ = 0;
  int64_t tighten_until_ms_ = 0;
  float base_mincutoff_ = 1.f;
};

}  // namespace cammouse
