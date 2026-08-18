#include "cammouse/input/backend.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace cammouse {
namespace {

void send_mouse(DWORD flags, LONG dx, LONG dy, DWORD data) {
  INPUT in{};
  in.type = INPUT_MOUSE;
  in.mi.dx = dx;
  in.mi.dy = dy;
  in.mi.mouseData = data;
  in.mi.dwFlags = flags;
  SendInput(1, &in, sizeof(INPUT));
}

}  // namespace

class SendInputBackend final : public InputBackend {
 public:
  void move_abs(int x, int y) override {
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
    const int vh = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
    const LONG nx = static_cast<LONG>(((x - vx) * 65535.0) / (vw - 1));
    const LONG ny = static_cast<LONG>(((y - vy) * 65535.0) / (vh - 1));
    send_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
               nx, ny, 0);
  }

  void button(Button button, bool down) override {
    DWORD flags = 0;
    if (button == Button::Left) {
      flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
      left_down_ = down;
    } else if (button == Button::Right) {
      flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
      right_down_ = down;
    } else {
      return;
    }
    send_mouse(flags, 0, 0, 0);
  }

  void scroll(int dx, int dy) override {
    if (dy != 0) send_mouse(MOUSEEVENTF_WHEEL, 0, 0, static_cast<DWORD>(dy * WHEEL_DELTA));
    if (dx != 0) send_mouse(MOUSEEVENTF_HWHEEL, 0, 0, static_cast<DWORD>(dx * WHEEL_DELTA));
  }

  void release_all() override {
    if (left_down_) button(Button::Left, false);
    if (right_down_) button(Button::Right, false);
  }

  std::string name() const override { return "sendinput"; }

 private:
  bool left_down_ = false;
  bool right_down_ = false;
};

std::unique_ptr<InputBackend> create_sendinput_backend() {
  return std::make_unique<SendInputBackend>();
}

}  // namespace cammouse

#endif
