#include "airmouse/platform/hotkey.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace airmouse {

class WinHotkey final : public Hotkey {
 public:
  ~WinHotkey() override {
    if (registered_) UnregisterHotKey(nullptr, 1);
  }
  bool grab() override {
    registered_ = RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'C');
    return registered_;
  }
  void poll() override {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_HOTKEY && handler_) handler_();
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  void set_handler(std::function<void()> on_toggle) override { handler_ = std::move(on_toggle); }

 private:
  bool registered_ = false;
  std::function<void()> handler_;
};

std::unique_ptr<Hotkey> create_hotkey() { return std::make_unique<WinHotkey>(); }

}  // namespace airmouse

#endif
