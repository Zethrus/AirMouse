#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "cammouse/app/tracker.hpp"
#include "cammouse/config.hpp"
#include "cammouse/input/factory.hpp"
#include "cammouse/platform/dbus_toggle.hpp"
#include "cammouse/platform/hotkey.hpp"
#include "cammouse/ui/overlay.hpp"
#include "cammouse/ui/settings.hpp"
#include "cammouse/ui/tray.hpp"

namespace {

std::atomic<bool> g_run{true};

void on_signal(int) { g_run = false; }

#ifdef _WIN32
void enable_dpi() {
  using Fn = BOOL(WINAPI*)(HANDLE);
  if (HMODULE user = GetModuleHandleW(L"user32.dll")) {
    if (auto fn = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
            GetProcAddress(user, "SetProcessDpiAwarenessContext"))) {
      fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
  }
}
#endif

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
#ifdef _WIN32
  enable_dpi();
#endif
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  cammouse::Config cfg = cammouse::default_config();
  const auto cfg_path = cammouse::default_config_path();
  try {
    cfg = cammouse::load_config(cfg_path);
  } catch (const std::exception& ex) {
    std::cerr << "config: " << ex.what() << '\n';
  }

  std::cerr << "CamMouse  session=" << cammouse::session_label(cammouse::detect_session())
            << "  assets=" << cammouse::asset_root().string() << '\n';

  auto overlay = cammouse::create_overlay();
  if (cfg.hud.enabled) {
    if (!overlay->create()) {
      std::cerr << "overlay: failed to create window\n";
    }
  }

  auto tray = cammouse::create_tray();
  tray->create();

  auto settings = cammouse::create_settings(&cfg);
  auto hotkey = cammouse::create_hotkey();
  hotkey->grab();
  auto dbus = cammouse::create_dbus_toggle();
  dbus->advertise();

  cammouse::Tracker tracker(cfg);
  const bool started = tracker.start();
  if (!started) {
    std::cerr << "tracker: " << tracker.last_error() << '\n';
  }

  cammouse::PoseName last_pose = cammouse::PoseName::None;
  const auto toggle = [&] {
    tracker.toggle_pause();
    tray->set_status(tracker.paused() ? "paused" : "tracking");
  };

  tray->set_handler([&](cammouse::TrayAction action) {
    switch (action) {
      case cammouse::TrayAction::TogglePause:
        toggle();
        break;
      case cammouse::TrayAction::ToggleHud:
        if (overlay->visible()) {
          overlay->set_visible(false);
        } else {
          if (!overlay->visible()) {
            overlay->create();
          }
          overlay->set_visible(true);
        }
        break;
      case cammouse::TrayAction::OpenSettings:
        settings->show();
        break;
      case cammouse::TrayAction::Calibrate:
        settings->show();
        break;
      case cammouse::TrayAction::Quit:
        g_run = false;
        break;
    }
  });
  hotkey->set_handler(toggle);
  dbus->set_handler(toggle);

  while (g_run) {
    const auto snap = tracker.snapshot();
    overlay->set_snapshot(snap);
    if (cfg.hud.chips && snap.pose != last_pose && snap.pose != cammouse::PoseName::None &&
        snap.pose != cammouse::PoseName::Point) {
      overlay->set_chip(std::string(cammouse::pose_label(snap.pose)));
    }
    last_pose = snap.pose;
    overlay->poll();
    tray->poll();
    settings->poll();
    hotkey->poll();
    dbus->poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  tracker.stop();
  overlay->destroy();
  try {
    cammouse::save_config(cfg_path, cfg);
  } catch (...) {
  }
  return started ? 0 : 1;
}
