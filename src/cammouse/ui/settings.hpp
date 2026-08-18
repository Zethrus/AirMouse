#pragma once

#include <functional>
#include <memory>

#include "cammouse/config.hpp"

namespace cammouse {

class SettingsWindow {
 public:
  virtual ~SettingsWindow() = default;
  virtual void show() = 0;
  virtual void hide() = 0;
  virtual bool visible() const = 0;
  virtual void poll() = 0;
  virtual void set_on_change(std::function<void(const Config&)> cb) = 0;
};

std::unique_ptr<SettingsWindow> create_settings(Config* cfg);

}  // namespace cammouse
