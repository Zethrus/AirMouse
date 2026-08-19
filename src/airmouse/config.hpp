#pragma once

#include <filesystem>
#include <string>

namespace airmouse {

struct SmoothingConfig {
  float mincutoff = 1.0f;
  float beta = 0.007f;
  float dcutoff = 1.0f;
  float landmark_mincutoff = 1.2f;
  float landmark_beta = 0.007f;
  float deadzone = 0.0015f;
};

struct EnhanceConfig {
  bool enabled = true;
  float target_luma = 112.f;
  float max_gain = 2.4f;
  float adapt_tau = 0.45f;
  float denoise_above = 2.0f;
};

struct TrackingConfig {
  float min_detection = 0.35f;
  float min_presence = 0.40f;
  float min_tracking = 0.30f;
  int lost_hold_ms = 180;
  int pinch_frames = 2;
};

struct HudConfig {
  bool enabled = true;
  bool chips = true;
  bool show_camera = false;
  bool placed = false;
  int x = 0;
  int y = 0;
};

struct GestureToggles {
  bool left_pinch = true;
  bool right_pinch = true;
  bool drag = true;
  bool scroll = true;
  bool clutch = true;
  bool lock = true;
  bool double_click = false;
  bool middle_click = false;
  bool two_hand_zoom = false;
};

struct Config {
  int camera_index = -1;  // -1 = first real capture node
  bool mirror = true;
  int frame_width = 640;
  int frame_height = 480;
  int fps = 30;
  float control_box = 0.62f;
  SmoothingConfig smoothing{};
  EnhanceConfig enhance{};
  TrackingConfig tracking{};
  std::string dominant_hand = "auto";  // auto | left | right
  std::string hotkey = "ctrl+alt+shift+c";
  HudConfig hud{};
  GestureToggles gestures{};
  float pinch_on = 0.35f;
  float pinch_off = 0.50f;
};

Config default_config();
Config load_config(const std::filesystem::path& path);
void save_config(const std::filesystem::path& path, const Config& cfg);
std::filesystem::path default_config_path();
std::filesystem::path asset_root();

}  // namespace airmouse
