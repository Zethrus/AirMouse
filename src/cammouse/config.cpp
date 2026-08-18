#include "cammouse/config.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

namespace cammouse {
namespace {

using nlohmann::json;

json to_json(const Config& cfg) {
  return json{
      {"camera_index", cfg.camera_index},
      {"mirror", cfg.mirror},
      {"frame_width", cfg.frame_width},
      {"frame_height", cfg.frame_height},
      {"fps", cfg.fps},
      {"control_box", cfg.control_box},
      {"smoothing",
       {{"mincutoff", cfg.smoothing.mincutoff},
        {"beta", cfg.smoothing.beta},
        {"dcutoff", cfg.smoothing.dcutoff}}},
      {"dominant_hand", cfg.dominant_hand},
      {"hotkey", cfg.hotkey},
      {"hud",
       {{"enabled", cfg.hud.enabled},
        {"chips", cfg.hud.chips},
        {"show_camera", cfg.hud.show_camera}}},
      {"gestures",
       {{"left_pinch", cfg.gestures.left_pinch},
        {"right_pinch", cfg.gestures.right_pinch},
        {"drag", cfg.gestures.drag},
        {"scroll", cfg.gestures.scroll},
        {"clutch", cfg.gestures.clutch},
        {"lock", cfg.gestures.lock},
        {"double_click", cfg.gestures.double_click},
        {"middle_click", cfg.gestures.middle_click},
        {"two_hand_zoom", cfg.gestures.two_hand_zoom}}},
      {"pinch_on", cfg.pinch_on},
      {"pinch_off", cfg.pinch_off},
  };
}

void merge(Config& cfg, const json& j) {
  if (j.contains("camera_index")) cfg.camera_index = j["camera_index"].get<int>();
  if (j.contains("mirror")) cfg.mirror = j["mirror"].get<bool>();
  if (j.contains("frame_width")) cfg.frame_width = j["frame_width"].get<int>();
  if (j.contains("frame_height")) cfg.frame_height = j["frame_height"].get<int>();
  if (j.contains("fps")) cfg.fps = j["fps"].get<int>();
  if (j.contains("control_box")) cfg.control_box = j["control_box"].get<float>();
  if (j.contains("smoothing")) {
    const auto& s = j["smoothing"];
    if (s.contains("mincutoff")) cfg.smoothing.mincutoff = s["mincutoff"].get<float>();
    if (s.contains("beta")) cfg.smoothing.beta = s["beta"].get<float>();
    if (s.contains("dcutoff")) cfg.smoothing.dcutoff = s["dcutoff"].get<float>();
  }
  if (j.contains("dominant_hand")) cfg.dominant_hand = j["dominant_hand"].get<std::string>();
  if (j.contains("hotkey")) cfg.hotkey = j["hotkey"].get<std::string>();
  if (j.contains("hud")) {
    const auto& h = j["hud"];
    if (h.contains("enabled")) cfg.hud.enabled = h["enabled"].get<bool>();
    if (h.contains("chips")) cfg.hud.chips = h["chips"].get<bool>();
    if (h.contains("show_camera")) cfg.hud.show_camera = h["show_camera"].get<bool>();
  }
  if (j.contains("gestures")) {
    const auto& g = j["gestures"];
    if (g.contains("left_pinch")) cfg.gestures.left_pinch = g["left_pinch"].get<bool>();
    if (g.contains("right_pinch")) cfg.gestures.right_pinch = g["right_pinch"].get<bool>();
    if (g.contains("drag")) cfg.gestures.drag = g["drag"].get<bool>();
    if (g.contains("scroll")) cfg.gestures.scroll = g["scroll"].get<bool>();
    if (g.contains("clutch")) cfg.gestures.clutch = g["clutch"].get<bool>();
    if (g.contains("lock")) cfg.gestures.lock = g["lock"].get<bool>();
    if (g.contains("double_click")) cfg.gestures.double_click = g["double_click"].get<bool>();
    if (g.contains("middle_click")) cfg.gestures.middle_click = g["middle_click"].get<bool>();
    if (g.contains("two_hand_zoom")) cfg.gestures.two_hand_zoom = g["two_hand_zoom"].get<bool>();
  }
  if (j.contains("pinch_on")) cfg.pinch_on = j["pinch_on"].get<float>();
  if (j.contains("pinch_off")) cfg.pinch_off = j["pinch_off"].get<float>();
}

}  // namespace

Config default_config() { return {}; }

Config load_config(const std::filesystem::path& path) {
  Config cfg = default_config();
  if (!std::filesystem::exists(path)) {
    return cfg;
  }
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot read config: " + path.string());
  }
  json j;
  in >> j;
  merge(cfg, j);
  return cfg;
}

void save_config(const std::filesystem::path& path, const Config& cfg) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("cannot write config: " + path.string());
  }
  out << to_json(cfg).dump(2) << '\n';
}

std::filesystem::path default_config_path() {
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  const std::filesystem::path base =
      appdata ? std::filesystem::path(appdata) : std::filesystem::path(".");
  return base / "CamMouse" / "config.json";
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  const char* home = std::getenv("HOME");
  std::filesystem::path base;
  if (xdg && *xdg) {
    base = xdg;
  } else if (home && *home) {
    base = std::filesystem::path(home) / ".config";
  } else {
    base = ".";
  }
  return base / "cammouse" / "config.json";
#endif
}

std::filesystem::path executable_dir() {
#ifdef _WIN32
  char buf[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (n == 0) return std::filesystem::current_path();
  return std::filesystem::path(buf).parent_path();
#else
  std::error_code ec;
  const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec) return std::filesystem::current_path();
  return exe.parent_path();
#endif
}

std::filesystem::path asset_root() {
  if (const char* env = std::getenv("CAMMOUSE_ASSETS"); env && *env) {
    return env;
  }
  const auto dir = executable_dir();
  const std::filesystem::path candidates[] = {
      dir / "assets",
      dir.parent_path() / "assets",
      dir.parent_path().parent_path() / "assets",
      std::filesystem::current_path() / "assets",
  };
  for (const auto& c : candidates) {
    if (std::filesystem::exists(c / "models" / "hand_landmarker.task")) {
      return c;
    }
  }
  return std::filesystem::current_path() / "assets";
}

}  // namespace cammouse
