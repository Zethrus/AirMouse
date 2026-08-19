#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <windows.h>
#endif

#include "airmouse/app/tracker.hpp"
#include "airmouse/config.hpp"
#include "airmouse/input/factory.hpp"
#include "airmouse/platform/dbus_toggle.hpp"
#include "airmouse/platform/hotkey.hpp"
#include "airmouse/ui/overlay.hpp"
#include "airmouse/ui/settings.hpp"
#include "airmouse/ui/tray.hpp"

namespace {

std::atomic<bool> g_run{true};

void on_signal(int) { g_run = false; }

#ifdef _WIN32
void enable_dpi() {
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
  const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool com_balanced = SUCCEEDED(com);
#endif
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  airmouse::Config cfg = airmouse::default_config();
  const auto cfg_path = airmouse::default_config_path();
  try {
    cfg = airmouse::load_config(cfg_path);
  } catch (const std::exception& ex) {
    std::cerr << "config: " << ex.what() << '\n';
  }

  std::cerr << "AirMouse  session=" << airmouse::session_label(airmouse::detect_session())
            << "  assets=" << airmouse::asset_root().string() << '\n';

  auto overlay = airmouse::create_overlay();
  overlay->set_placement(cfg.hud);
  overlay->set_on_moved([&](airmouse::HudConfig next) {
    cfg.hud.placed = next.placed;
    cfg.hud.x = next.x;
    cfg.hud.y = next.y;
    try {
      airmouse::save_config(cfg_path, cfg);
    } catch (...) {
    }
  });
  if (cfg.hud.enabled) {
    if (!overlay->create()) {
      std::cerr << "overlay: failed to create window\n";
    }
  }

  auto tray = airmouse::create_tray();
  tray->create();

  auto settings = airmouse::create_settings(&cfg);
  auto hotkey = airmouse::create_hotkey();
  hotkey->grab();
  auto dbus = airmouse::create_dbus_toggle();
  dbus->advertise();

  airmouse::Tracker tracker(cfg);
  tracker.start();
  if (!tracker.last_error().empty()) {
    std::cerr << "tracker: " << tracker.last_error() << '\n';
  }

  airmouse::PoseName last_pose = airmouse::PoseName::None;
  const auto toggle = [&] {
    tracker.toggle_pause();
    tray->set_status(tracker.paused() ? "paused" : "tracking");
  };

  tray->set_handler([&](airmouse::TrayAction action) {
    switch (action) {
      case airmouse::TrayAction::TogglePause:
        toggle();
        break;
      case airmouse::TrayAction::ToggleHud:
        if (overlay->visible()) {
          overlay->set_visible(false);
        } else {
          if (!overlay->visible()) {
            overlay->create();
          }
          overlay->set_visible(true);
        }
        break;
      case airmouse::TrayAction::OpenSettings:
        settings->show();
        break;
      case airmouse::TrayAction::Calibrate:
        settings->show();
        break;
      case airmouse::TrayAction::Quit:
        g_run = false;
        break;
    }
  });
  hotkey->set_handler(toggle);
  dbus->set_handler(toggle);
  settings->set_on_change([&](const airmouse::Config& next) {
    cfg = next;
    tracker.set_config(cfg);
    overlay->set_placement(cfg.hud);
    if (cfg.hud.enabled) {
      overlay->create();
      overlay->set_visible(true);
    } else {
      overlay->set_visible(false);
    }
  });

  while (g_run) {
    const auto snap = tracker.snapshot();
    overlay->set_snapshot(snap);
    if (cfg.hud.chips && snap.pose != last_pose && snap.pose != airmouse::PoseName::None &&
        snap.pose != airmouse::PoseName::Point) {
      overlay->set_chip(std::string(airmouse::pose_label(snap.pose)));
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
    airmouse::save_config(cfg_path, cfg);
  } catch (...) {
  }
#ifdef _WIN32
  if (com_balanced) CoUninitialize();
#endif
  return 0;
}
