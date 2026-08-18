#include "airmouse/ui/tray.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "airmouse/ui/icon.hpp"

#pragma comment(lib, "shell32.lib")

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif

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

HICON load_file_icon(bool* owned) {
  *owned = false;
  const auto ico = app_icon_path(32);
  if (ico.empty()) return nullptr;
  HICON icon = static_cast<HICON>(
      LoadImageW(nullptr, ico.wstring().c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
  if (icon) *owned = true;
  return icon;
}

HICON make_app_icon(bool* owned) {
  if (HICON file = load_file_icon(owned)) return file;
  *owned = false;
  constexpr int s = 32;
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = s;
  bmi.bmiHeader.biHeight = -s;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dc = GetDC(nullptr);
  HBITMAP color = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!color || !bits) {
    if (dc) ReleaseDC(nullptr, dc);
    return LoadIconW(nullptr, IDI_APPLICATION);
  }
  auto* px = static_cast<std::uint32_t*>(bits);
  for (int y = 0; y < s; ++y) {
    for (int x = 0; x < s; ++x) {
      const int cx = x - s / 2;
      const int cy = y - s / 2;
      const int r2 = cx * cx + cy * cy;
      if (r2 <= 13 * 13) {
        px[y * s + x] = 0xFFC4A574;  // BGRA of accent #C4A574
      } else if (r2 <= 15 * 15) {
        px[y * s + x] = 0xFF0B0C0F;
      } else {
        px[y * s + x] = 0x00000000;
      }
    }
  }
  HBITMAP mask = CreateBitmap(s, s, 1, 1, nullptr);
  ICONINFO ii{};
  ii.fIcon = TRUE;
  ii.hbmMask = mask;
  ii.hbmColor = color;
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(color);
  if (mask) DeleteObject(mask);
  ReleaseDC(nullptr, dc);
  if (!icon) return LoadIconW(nullptr, IDI_APPLICATION);
  *owned = true;
  return icon;
}

class WinTray final : public Tray {
 public:
  ~WinTray() override {
    if (added_) {
      Shell_NotifyIconW(NIM_DELETE, &nid_);
    }
    if (hwnd_) DestroyWindow(hwnd_);
    if (icon_owned_ && icon_) {
      DestroyIcon(icon_);
      icon_ = nullptr;
    }
  }

  bool create() override {
    taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinTray::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseTray";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }

    hwnd_ = CreateWindowExW(0, L"AirMouseTray", L"AirMouse", WS_OVERLAPPED, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    if (taskbar_created_) {
      using FilterFn = BOOL(WINAPI*)(HWND, UINT, DWORD, void*);
      if (HMODULE user = GetModuleHandleW(L"user32.dll")) {
        if (auto fn = reinterpret_cast<FilterFn>(
                GetProcAddress(user, "ChangeWindowMessageFilterEx"))) {
          fn(hwnd_, taskbar_created_, MSGFLT_ALLOW, nullptr);
        }
      }
    }

    icon_ = make_app_icon(&icon_owned_);
    return add_icon();
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
    if (!hwnd_) return;
    MSG msg;
    while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

 private:
  bool add_icon() {
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = kTrayId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAY;
    nid_.hIcon = icon_;
    const auto w = widen(tooltip_);
    wcsncpy_s(nid_.szTip, w.c_str(), _TRUNCATE);
    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    if (added_) {
      nid_.uVersion = NOTIFYICON_VERSION_4;
      Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }
    return added_;
  }

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
    if (taskbar_created_ && msg == taskbar_created_) {
      add_icon();
      return 0;
    }
    if (msg == WM_TRAY) {
      const UINT ev = LOWORD(lparam);
      if (ev == WM_LBUTTONUP || ev == NIN_SELECT || ev == NIN_KEYSELECT) {
        if (handler_) handler_(TrayAction::TogglePause);
      } else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
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
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_,
                   nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
  }

  HWND hwnd_ = nullptr;
  HICON icon_ = nullptr;
  bool icon_owned_ = false;
  NOTIFYICONDATAW nid_{};
  UINT taskbar_created_ = 0;
  bool added_ = false;
  std::function<void(TrayAction)> handler_;
  std::string tooltip_ = "AirMouse";
};

}  // namespace

std::unique_ptr<Tray> create_tray() { return std::make_unique<WinTray>(); }

}  // namespace airmouse

#endif
