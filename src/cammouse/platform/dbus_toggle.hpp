#pragma once

#include <functional>
#include <memory>

namespace cammouse {

class DbusToggle {
 public:
  virtual ~DbusToggle() = default;
  virtual bool advertise() = 0;
  virtual void poll() = 0;
  virtual void set_handler(std::function<void()> on_toggle) = 0;
};

std::unique_ptr<DbusToggle> create_dbus_toggle();

}  // namespace cammouse
