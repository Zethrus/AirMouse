#include "cammouse/app/tracker.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <thread>

#include "cammouse/input/factory.hpp"
#include "cammouse/pointer/screens.hpp"

namespace cammouse {
namespace {

int64_t now_ms() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch())
      .count();
}

}  // namespace

Tracker::Tracker(Config cfg)
    : cfg_(std::move(cfg)),
      engine_(cfg_),
      filter_(cfg_.smoothing.mincutoff, cfg_.smoothing.beta, cfg_.smoothing.dcutoff),
      camera_(create_camera()),
      landmarker_(create_mediapipe_landmarker()) {
  mapper_.control_box = cfg_.control_box;
  base_mincutoff_ = cfg_.smoothing.mincutoff;
}

Tracker::~Tracker() { stop(); }

bool Tracker::start() {
  screen_ = query_screen_geometry();
  if (!camera_->open(cfg_.camera_index, cfg_.frame_width, cfg_.frame_height, cfg_.fps)) {
    std::lock_guard<std::mutex> lock(mu_);
    error_ = camera_->last_error().empty() ? "camera open failed" : camera_->last_error();
    snap_.status = "NO CAM";
    snap_.camera_ok = false;
    return false;
  }
  const auto model = asset_root() / "models" / "hand_landmarker.task";
  if (!landmarker_->open(model)) {
    std::lock_guard<std::mutex> lock(mu_);
    error_ = landmarker_->last_error();
    snap_.status = "NO MODEL";
    return false;
  }
  try {
    input_ = create_input_backend();
  } catch (const std::exception& ex) {
    std::lock_guard<std::mutex> lock(mu_);
    error_ = ex.what();
    snap_.status = "NO INPUT";
    return false;
  }
  running_ = true;
  thread_ = std::thread([this] { loop(); });
  return true;
}

void Tracker::stop() {
  running_ = false;
  if (thread_.joinable()) thread_.join();
  if (input_) input_->release_all();
  if (camera_) camera_->close();
}

void Tracker::set_paused(bool paused) {
  paused_ = paused;
  if (paused && input_) input_->release_all();
}

void Tracker::set_config(const Config& cfg) {
  cfg_ = cfg;
  engine_.set_config(cfg_);
  mapper_.control_box = cfg_.control_box;
  filter_.set_params(cfg_.smoothing.mincutoff, cfg_.smoothing.beta, cfg_.smoothing.dcutoff);
  base_mincutoff_ = cfg_.smoothing.mincutoff;
}

TrackingSnapshot Tracker::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  return snap_;
}

std::string Tracker::last_error() const {
  std::lock_guard<std::mutex> lock(mu_);
  return error_;
}

void Tracker::apply(const PointerCommand& cmd, float dt) {
  if (!input_ || paused_) return;
  if (cmd.release_all) {
    input_->release_all();
    return;
  }
  if (cmd.has_target) {
    if (tighten_until_ms_ != 0 && now_ms() > tighten_until_ms_) {
      filter_.set_params(base_mincutoff_, cfg_.smoothing.beta, cfg_.smoothing.dcutoff);
      tighten_until_ms_ = 0;
    }
    const Vec2 filtered = filter_.filter({cmd.nx, cmd.ny}, dt);
    const Vec2 pt = mapper_.map(filtered.x, filtered.y, screen_);
    input_->move_abs(static_cast<int>(pt.x), static_cast<int>(pt.y));
  }
  if (cmd.press != Button::Idle) {
    input_->button(cmd.press, true);
    filter_.tighten(base_mincutoff_ * 1.8f);
    tighten_until_ms_ = now_ms() + 80;
  }
  if (cmd.release != Button::Idle) {
    input_->button(cmd.release, false);
  }
  if (cmd.wheel != 0) {
    input_->scroll(0, cmd.wheel);
  }
}

void Tracker::loop() {
  RgbFrame frame;
  last_ms_ = now_ms();
  int frames = 0;
  int64_t fps_t = last_ms_;
  float fps = 0;
  while (running_) {
    const int64_t t = now_ms();
    const float dt = std::max(0.001f, static_cast<float>(t - last_ms_) / 1000.f);
    last_ms_ = t;

    const bool got = camera_->read(frame);
    std::optional<HandFrame> hand;
    if (got) {
      auto hands = landmarker_->detect(frame, t);
      if (!hands.empty()) {
        hand = hands.front();
        if (cfg_.dominant_hand == "auto") {
          for (const auto& h : hands) {
            if (h.side == HandSide::Right) {
              hand = h;
              break;
            }
          }
        }
      }
      ++frames;
    }
    if (t - fps_t >= 1000) {
      fps = static_cast<float>(frames);
      frames = 0;
      fps_t = t;
    }

    PointerCommand cmd{};
    if (!paused_) {
      cmd = engine_.update(hand, t);
      apply(cmd, dt);
    } else {
      cmd.pose = PoseName::None;
    }

    TrackingSnapshot snap;
    snap.hand = hand;
    snap.command = cmd;
    snap.pose = paused_ ? PoseName::None : cmd.pose;
    snap.fps = fps;
    snap.camera_ok = camera_->ok();
    snap.status = paused_ ? "PAUSED" : pose_label(snap.pose);
    {
      std::lock_guard<std::mutex> lock(mu_);
      snap_ = snap;
    }

    if (!got) {
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  }
}

}  // namespace cammouse
