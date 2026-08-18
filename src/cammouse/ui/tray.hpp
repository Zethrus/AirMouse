#pragma once

#include <functional>
#include <memory>
#include <string>

namespace cammouse {

enum class TrayAction { TogglePause, ToggleHud, OpenSettings, Calibrate, Quit };

class Tray {
 public:
  virtual ~Tray() = default;
  virtual bool create() = 0;
  virtual void set_status(const std::string& text) = 0;
  virtual void poll() = 0;
  virtual void set_handler(std::function<void(TrayAction)> handler) = 0;
};

std::unique_ptr<Tray> create_tray();

}  // namespace cammouse
