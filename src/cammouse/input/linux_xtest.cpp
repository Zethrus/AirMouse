#include "cammouse/input/backend.hpp"

#ifndef _WIN32

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <stdexcept>

namespace cammouse {
namespace {

int xtest_button(Button button) {
  switch (button) {
    case Button::Left:
      return 1;
    case Button::Right:
      return 3;
    case Button::Idle:
      break;
  }
  return 0;
}

}  // namespace

class XTestBackend final : public InputBackend {
 public:
  XTestBackend() {
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
      throw std::runtime_error("XTest: cannot open X display");
    }
    int event_base = 0;
    int error_base = 0;
    int major = 0;
    int minor = 0;
    if (!XTestQueryExtension(dpy_, &event_base, &error_base, &major, &minor)) {
      XCloseDisplay(dpy_);
      dpy_ = nullptr;
      throw std::runtime_error("XTest extension is not available");
    }
  }

  ~XTestBackend() override {
    if (dpy_) {
      release_all();
      XCloseDisplay(dpy_);
    }
  }

  void move_abs(int x, int y) override {
    XTestFakeMotionEvent(dpy_, -1, x, y, CurrentTime);
    XFlush(dpy_);
    last_x_ = x;
    last_y_ = y;
  }

  void button(Button button, bool down) override {
    const int b = xtest_button(button);
    if (b == 0) return;
    XTestFakeButtonEvent(dpy_, static_cast<unsigned int>(b), down ? True : False,
                         CurrentTime);
    XFlush(dpy_);
    if (button == Button::Left) left_down_ = down;
    if (button == Button::Right) right_down_ = down;
  }

  void scroll(int dx, int dy) override {
    auto tick = [&](int btn, int count) {
      for (int i = 0; i < count; ++i) {
        XTestFakeButtonEvent(dpy_, static_cast<unsigned int>(btn), True, CurrentTime);
        XTestFakeButtonEvent(dpy_, static_cast<unsigned int>(btn), False, CurrentTime);
      }
    };
    if (dy > 0) tick(4, dy);
    if (dy < 0) tick(5, -dy);
    if (dx > 0) tick(7, dx);
    if (dx < 0) tick(6, -dx);
    if (dx != 0 || dy != 0) XFlush(dpy_);
  }

  void release_all() override {
    if (left_down_) button(Button::Left, false);
    if (right_down_) button(Button::Right, false);
  }

  std::string name() const override { return "xtest"; }

 private:
  Display* dpy_ = nullptr;
  int last_x_ = 0;
  int last_y_ = 0;
  bool left_down_ = false;
  bool right_down_ = false;
};

std::unique_ptr<InputBackend> create_xtest_backend() {
  return std::make_unique<XTestBackend>();
}

}  // namespace cammouse

#endif
