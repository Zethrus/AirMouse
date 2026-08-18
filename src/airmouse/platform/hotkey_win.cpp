#include "airmouse/platform/hotkey.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace airmouse {
namespace {

class WinHotkey final : public Hotkey {
 public:
  ~WinHotkey() override {
    if (registered_) UnregisterHotKey(hwnd_, 1);
    if (hwnd_) DestroyWindow(hwnd_);
  }

  bool grab() override {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinHotkey::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseHotkey";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }
    hwnd_ = CreateWindowExW(0, L"AirMouseHotkey", L"AirMouseHotkey", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;
    registered_ = RegisterHotKey(hwnd_, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'C') == TRUE;
    return registered_;
  }

  void poll() override {
    if (!hwnd_) return;
    MSG msg;
    while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  void set_handler(std::function<void()> on_toggle) override { handler_ = std::move(on_toggle); }

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WinHotkey* self = nullptr;
    if (msg == WM_NCCREATE) {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<WinHotkey*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<WinHotkey*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handle(hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  LRESULT handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_HOTKEY && handler_) {
      handler_();
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  HWND hwnd_ = nullptr;
  bool registered_ = false;
  std::function<void()> handler_;
};

}  // namespace

std::unique_ptr<Hotkey> create_hotkey() { return std::make_unique<WinHotkey>(); }

}  // namespace airmouse

#endif
