#include "airmouse/ui/settings.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <windows.h>

#include "airmouse/ui/icon.hpp"

namespace airmouse {
namespace {

class WinSettings final : public SettingsWindow {
 public:
  explicit WinSettings(Config* cfg) : cfg_(cfg) {}
  ~WinSettings() override { hide(); }

  void show() override {
    if (hwnd_) {
      ShowWindow(hwnd_, SW_SHOW);
      SetForegroundWindow(hwnd_);
      visible_ = true;
      return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinSettings::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseSettings";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    const auto ico = app_icon_path(32);
    if (!ico.empty()) {
      wc.hIcon = static_cast<HICON>(LoadImageW(nullptr, ico.wstring().c_str(), IMAGE_ICON, 32, 32,
                                               LR_LOADFROMFILE));
    }
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, L"AirMouseSettings", L"AirMouse settings",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                            420, 280, nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return;
    hud_ = CreateWindowW(L"BUTTON", L"Show HUD", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 24,
                         200, 24, hwnd_, reinterpret_cast<HMENU>(1), GetModuleHandleW(nullptr),
                         nullptr);
    chips_ = CreateWindowW(L"BUTTON", L"Gesture chips", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24,
                           56, 200, 24, hwnd_, reinterpret_cast<HMENU>(2),
                           GetModuleHandleW(nullptr), nullptr);
    mirror_ = CreateWindowW(L"BUTTON", L"Mirror camera", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24,
                            88, 200, 24, hwnd_, reinterpret_cast<HMENU>(3),
                            GetModuleHandleW(nullptr), nullptr);
    CreateWindowW(L"BUTTON", L"Open config folder", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 140,
                  170, 32, hwnd_, reinterpret_cast<HMENU>(10), GetModuleHandleW(nullptr), nullptr);
    CreateWindowW(L"BUTTON", L"Done", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 280, 140, 90, 32,
                  hwnd_, reinterpret_cast<HMENU>(11), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hud_, BM_SETCHECK, cfg_->hud.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chips_, BM_SETCHECK, cfg_->hud.chips ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(mirror_, BM_SETCHECK, cfg_->mirror ? BST_CHECKED : BST_UNCHECKED, 0);
    ShowWindow(hwnd_, SW_SHOW);
    visible_ = true;
  }

  void hide() override {
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    visible_ = false;
  }
  bool visible() const override { return visible_; }
  void poll() override {
    if (!hwnd_) return;
    MSG msg;
    while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  void set_on_change(std::function<void(const Config&)> cb) override { cb_ = std::move(cb); }

 private:
  void apply() {
    cfg_->hud.enabled = SendMessageW(hud_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_->hud.chips = SendMessageW(chips_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg_->mirror = SendMessageW(mirror_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    try {
      save_config(default_config_path(), *cfg_);
    } catch (...) {
    }
    if (cb_) cb_(*cfg_);
  }

  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WinSettings* self = nullptr;
    if (msg == WM_NCCREATE) {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<WinSettings*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<WinSettings*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handle(hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  LRESULT handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_COMMAND) {
      const int id = LOWORD(wparam);
      if (id == 1 || id == 2 || id == 3) apply();
      if (id == 10) {
        apply();
        const auto path = default_config_path().parent_path();
        ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      if (id == 11) {
        apply();
        hide();
      }
      return 0;
    }
    if (msg == WM_CLOSE) {
      apply();
      hide();
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  Config* cfg_ = nullptr;
  std::function<void(const Config&)> cb_;
  HWND hwnd_ = nullptr;
  HWND hud_ = nullptr;
  HWND chips_ = nullptr;
  HWND mirror_ = nullptr;
  bool visible_ = false;
};

}  // namespace

std::unique_ptr<SettingsWindow> create_win_settings(Config* cfg) {
  return std::make_unique<WinSettings>(cfg);
}

}  // namespace airmouse

#endif
