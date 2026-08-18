#pragma once

#include <functional>
#include <memory>

namespace airmouse {

class Hotkey {
 public:
  virtual ~Hotkey() = default;
  virtual bool grab() = 0;
  virtual void poll() = 0;
  virtual void set_handler(std::function<void()> on_toggle) = 0;
};

std::unique_ptr<Hotkey> create_hotkey();

}  // namespace airmouse
