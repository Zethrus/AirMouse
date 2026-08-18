#pragma once

#include <memory>
#include <string>

#include "airmouse/types.hpp"

namespace airmouse {

class InputBackend {
 public:
  virtual ~InputBackend() = default;
  virtual void move_abs(int x, int y) = 0;
  virtual void button(Button button, bool down) = 0;
  virtual void scroll(int dx, int dy) = 0;
  virtual void release_all() = 0;
  virtual std::string name() const = 0;
};

}  // namespace airmouse
