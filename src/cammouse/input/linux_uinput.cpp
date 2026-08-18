#include "cammouse/input/backend.hpp"

#ifndef _WIN32

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace cammouse {
namespace {

void ioctl_or_throw(int fd, unsigned long req, int arg, const char* what) {
  if (ioctl(fd, req, arg) < 0) {
    throw std::runtime_error(std::string("uinput: ") + what);
  }
}

}  // namespace

class UInputBackend final : public InputBackend {
 public:
  UInputBackend() {
    fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd_ < 0) {
      throw std::runtime_error(
          "uinput: cannot open /dev/uinput (add yourself to the input group "
          "and install the udev rule)");
    }
    ioctl_or_throw(fd_, UI_SET_EVBIT, EV_KEY, "EV_KEY");
    ioctl_or_throw(fd_, UI_SET_EVBIT, EV_REL, "EV_REL");
    ioctl_or_throw(fd_, UI_SET_EVBIT, EV_SYN, "EV_SYN");
    ioctl_or_throw(fd_, UI_SET_KEYBIT, BTN_LEFT, "BTN_LEFT");
    ioctl_or_throw(fd_, UI_SET_KEYBIT, BTN_RIGHT, "BTN_RIGHT");
    ioctl_or_throw(fd_, UI_SET_KEYBIT, BTN_MIDDLE, "BTN_MIDDLE");
    ioctl_or_throw(fd_, UI_SET_RELBIT, REL_X, "REL_X");
    ioctl_or_throw(fd_, UI_SET_RELBIT, REL_Y, "REL_Y");
    ioctl_or_throw(fd_, UI_SET_RELBIT, REL_WHEEL, "REL_WHEEL");
    ioctl_or_throw(fd_, UI_SET_RELBIT, REL_HWHEEL, "REL_HWHEEL");

    uinput_setup setup{};
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0xCA3;
    setup.id.product = 0x0001;
    std::snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "CamMouse virtual pointer");
    if (ioctl(fd_, UI_DEV_SETUP, &setup) < 0) {
      close(fd_);
      fd_ = -1;
      throw std::runtime_error("uinput: UI_DEV_SETUP failed");
    }
    if (ioctl(fd_, UI_DEV_CREATE) < 0) {
      close(fd_);
      fd_ = -1;
      throw std::runtime_error("uinput: UI_DEV_CREATE failed");
    }
    primed_ = false;
  }

  ~UInputBackend() override {
    if (fd_ >= 0) {
      release_all();
      ioctl(fd_, UI_DEV_DESTROY);
      close(fd_);
    }
  }

  void move_abs(int x, int y) override {
    if (!primed_) {
      last_x_ = x;
      last_y_ = y;
      primed_ = true;
      return;
    }
    const int dx = x - last_x_;
    const int dy = y - last_y_;
    last_x_ = x;
    last_y_ = y;
    if (dx == 0 && dy == 0) return;
    emit(EV_REL, REL_X, dx);
    emit(EV_REL, REL_Y, dy);
    emit(EV_SYN, SYN_REPORT, 0);
  }

  void button(Button button, bool down) override {
    int code = 0;
    if (button == Button::Left) {
      code = BTN_LEFT;
      left_down_ = down;
    } else if (button == Button::Right) {
      code = BTN_RIGHT;
      right_down_ = down;
    } else {
      return;
    }
    emit(EV_KEY, code, down ? 1 : 0);
    emit(EV_SYN, SYN_REPORT, 0);
  }

  void scroll(int dx, int dy) override {
    if (dy != 0) emit(EV_REL, REL_WHEEL, dy);
    if (dx != 0) emit(EV_REL, REL_HWHEEL, dx);
    if (dx != 0 || dy != 0) emit(EV_SYN, SYN_REPORT, 0);
  }

  void release_all() override {
    if (left_down_) button(Button::Left, false);
    if (right_down_) button(Button::Right, false);
  }

  std::string name() const override { return "uinput"; }

 private:
  void emit(int type, int code, int value) {
    input_event ev{};
    ev.type = static_cast<uint16_t>(type);
    ev.code = static_cast<uint16_t>(code);
    ev.value = value;
    if (write(fd_, &ev, sizeof(ev)) != static_cast<ssize_t>(sizeof(ev))) {
      // Best-effort; the device may have been yanked.
    }
  }

  int fd_ = -1;
  int last_x_ = 0;
  int last_y_ = 0;
  bool primed_ = false;
  bool left_down_ = false;
  bool right_down_ = false;
};

std::unique_ptr<InputBackend> create_uinput_backend() {
  return std::make_unique<UInputBackend>();
}

}  // namespace cammouse

#endif
