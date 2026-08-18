#include "airmouse/ui/tray.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace airmouse {
namespace {

constexpr UINT WM_TRAY = WM_APP + 7;
constexpr UINT kTrayId = 1;

std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

class WinTray final : public Tray {
 public:
  ~WinTray() override {
    if (added_) {
      Shell_NotifyIconW(NIM_DELETE, &nid_);
    }
    if (hwnd_) DestroyWindow(hwnd_);
  }

  bool create() override {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinTray::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseTray";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, L"AirMouseTray", L"AirMouse", WS_OVERLAPPED, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = kTrayId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAY;
    nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(nid_.szTip, L"AirMouse", _TRUNCATE);
    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    return added_;
  }

  void set_status(const std::string& text) override {
    tooltip_ = text.empty() ? "AirMouse" : ("AirMouse — " + text);
    if (!added_) return;
    const auto w = widen(tooltip_);
    wcsncpy_s(nid_.szTip, w.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  }

  void set_handler(std::function<void(TrayAction)> handler) override {
    handler_ = std::move(handler);
  }

  void poll() override {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WinTray* self = nullptr;
    if (msg == WM_NCCREATE) {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<WinTray*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<WinTray*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handle(hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  LRESULT handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_TRAY) {
      if (lparam == WM_LBUTTONUP) {
        if (handler_) handler_(TrayAction::TogglePause);
      } else if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
        show_menu();
      }
      return 0;
    }
    if (msg == WM_COMMAND) {
      switch (LOWORD(wparam)) {
        case 1:
          if (handler_) handler_(TrayAction::TogglePause);
          break;
        case 2:
          if (handler_) handler_(TrayAction::ToggleHud);
          break;
        case 3:
          if (handler_) handler_(TrayAction::OpenSettings);
          break;
        case 4:
          if (handler_) handler_(TrayAction::Calibrate);
          break;
        case 5:
          if (handler_) handler_(TrayAction::Quit);
          break;
        default:
          break;
      }
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  void show_menu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Pause / Resume");
    AppendMenuW(menu, MF_STRING, 2, L"Toggle HUD");
    AppendMenuW(menu, MF_STRING, 3, L"Settings");
    AppendMenuW(menu, MF_STRING, 4, L"Calibrate");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 5, L"Quit");
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
  }

  HWND hwnd_ = nullptr;
  NOTIFYICONDATAW nid_{};
  bool added_ = false;
  std::function<void(TrayAction)> handler_;
  std::string tooltip_ = "AirMouse";
};

}  // namespace

std::unique_ptr<Tray> create_tray() { return std::make_unique<WinTray>(); }

}  // namespace airmouse

#endif
