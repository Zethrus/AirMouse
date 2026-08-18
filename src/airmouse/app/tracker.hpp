#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "airmouse/camera/capture.hpp"
#include "airmouse/config.hpp"
#include "airmouse/gestures/engine.hpp"
#include "airmouse/input/backend.hpp"
#include "airmouse/pointer/filter.hpp"
#include "airmouse/pointer/mapper.hpp"
#include "airmouse/tracking/landmarker.hpp"
#include "airmouse/types.hpp"

namespace airmouse {

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
  std::mutex start_mu_;
  std::condition_variable start_cv_;
  bool start_done_ = false;
  bool start_ok_ = false;
  TrackingSnapshot snap_{};
  std::string error_;
  int64_t last_ms_ = 0;
  int64_t tighten_until_ms_ = 0;
  float base_mincutoff_ = 1.f;
};

}  // namespace airmouse
