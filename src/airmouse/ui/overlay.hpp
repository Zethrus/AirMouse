#pragma once

#include <memory>
#include <string>

#include "airmouse/types.hpp"

namespace airmouse {

class Overlay {
 public:
  virtual ~Overlay() = default;
  virtual bool create() = 0;
  virtual void destroy() = 0;
  virtual void set_visible(bool on) = 0;
  virtual bool visible() const = 0;
  virtual void set_snapshot(const TrackingSnapshot& snap) = 0;
  virtual void set_chip(std::string text) = 0;
  virtual void poll() = 0;
  virtual bool wants_quit() const = 0;
};

std::unique_ptr<Overlay> create_overlay();

}  // namespace airmouse
