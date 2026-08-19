#include "airmouse/ui/settings.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "airmouse/ui/gdi_draw.hpp"
#include "airmouse/ui/icon.hpp"
#include "airmouse/ui/settings_layout.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::PixelOffsetModeHighQuality;
using Gdiplus::SolidBrush;

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
    if (gdiplus_token_ == 0) {
      Gdiplus::GdiplusStartupInput input;
      if (Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr) != Gdiplus::Ok) return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinSettings::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseSettings";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    const auto ico = app_icon_path(32);
    if (!ico.empty()) {
      wc.hIcon = static_cast<HICON>(LoadImageW(nullptr, ico.wstring().c_str(), IMAGE_ICON, 32, 32,
                                               LR_LOADFROMFILE));
    }
    RegisterClassExW(&wc);
    RECT rc{0, 0, theme::settings::w, theme::settings::h};
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_APPWINDOW);
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, L"AirMouseSettings", L"AirMouse settings",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr,
                            GetModuleHandleW(nullptr), this);
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_SHOW);
    visible_ = true;
  }

  void hide() override {
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    visible_ = false;
    if (gdiplus_token_) {
      Gdiplus::GdiplusShutdown(gdiplus_token_);
      gdiplus_token_ = 0;
    }
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
    try {
      save_config(default_config_path(), *cfg_);
    } catch (...) {
    }
    if (cb_) cb_(*cfg_);
  }

  void on_click(int x, int y) {
    if (!cfg_) return;
    for (const auto& h : settings_hits()) {
      if (!hit_contains(h, x, y)) continue;
      if (std::strcmp(h.id, "hud") == 0) cfg_->hud.enabled = !cfg_->hud.enabled;
      else if (std::strcmp(h.id, "chips") == 0) cfg_->hud.chips = !cfg_->hud.chips;
      else if (std::strcmp(h.id, "mirror") == 0) cfg_->mirror = !cfg_->mirror;
      else if (std::strcmp(h.id, "reset") == 0) {
        cfg_->hud.placed = false;
        cfg_->hud.x = 0;
        cfg_->hud.y = 0;
      } else if (std::strcmp(h.id, "box") == 0) {
        const float t = std::clamp((x - h.x) / static_cast<float>(h.w), 0.f, 1.f);
        cfg_->control_box = 0.35f + t * 0.55f;
      } else if (std::strcmp(h.id, "open") == 0) {
        apply();
        const auto path = default_config_path().parent_path();
        ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      } else if (std::strcmp(h.id, "done") == 0) {
        apply();
        hide();
        return;
      }
      apply();
      InvalidateRect(hwnd_, nullptr, FALSE);
      return;
    }
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
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      paint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    if (msg == WM_LBUTTONUP) {
      on_click(static_cast<int>(static_cast<short>(LOWORD(lparam))),
               static_cast<int>(static_cast<short>(HIWORD(lparam))));
      return 0;
    }
    if (msg == WM_CLOSE) {
      apply();
      hide();
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  void paint(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    SolidBrush bg(draw::argb(theme::color::panel, 1.0));
    g.FillRectangle(&bg, 0, 0, theme::settings::w, theme::settings::h);

    Font title(L"Segoe UI", theme::type::title, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Font mono(L"Consolas", theme::type::body, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Font micro(L"Consolas", theme::type::micro, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Font label(L"Consolas", theme::type::label, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    const float pad = static_cast<float>(theme::space::xl);
    draw::label(g, title, theme::color::paper, theme::alpha::text, pad, 18, L"AirMouse");
    draw::label(g, label, theme::color::dim, theme::alpha::muted, pad, 44,
                L"POINTER  CAMERA  OVERLAY");

    auto chip = [&](const SettingsHit& hit, const std::wstring& text, bool on) {
      draw::fill_round_rect(g, static_cast<float>(hit.x), static_cast<float>(hit.y),
                            static_cast<float>(hit.w), static_cast<float>(hit.h),
                            static_cast<float>(theme::radius::chip),
                            on ? theme::color::accent : theme::color::hair,
                            on ? theme::alpha::chip_on : theme::alpha::chip_off);
      draw::label(g, mono, on ? theme::color::accent : theme::color::paper, theme::alpha::text,
                  static_cast<float>(hit.x + theme::space::md), static_cast<float>(hit.y + 3),
                  text.c_str());
    };

    for (const auto& h : settings_hits()) {
      if (std::strcmp(h.id, "cam") == 0) chip(h, L"Camera  Auto", false);
      else if (std::strcmp(h.id, "hud") == 0)
        chip(h, cfg_->hud.enabled ? L"HUD  on" : L"HUD  off", cfg_->hud.enabled);
      else if (std::strcmp(h.id, "chips") == 0)
        chip(h, cfg_->hud.chips ? L"Chips  on" : L"Chips  off", cfg_->hud.chips);
      else if (std::strcmp(h.id, "mirror") == 0)
        chip(h, cfg_->mirror ? L"Mirror  on" : L"Mirror  off", cfg_->mirror);
      else if (std::strcmp(h.id, "reset") == 0)
        chip(h, L"Reset HUD position", false);
      else if (std::strcmp(h.id, "box") == 0) {
        draw::label(g, label, theme::color::dim, theme::alpha::muted, static_cast<float>(h.x),
                    static_cast<float>(h.y - 16), L"Control box");
        draw::fill_round_rect(g, static_cast<float>(h.x), static_cast<float>(h.y),
                              static_cast<float>(h.w), static_cast<float>(h.h),
                              static_cast<float>(theme::radius::chip), theme::color::hair,
                              theme::alpha::hair_dim);
        const float t = (cfg_->control_box - 0.35f) / 0.55f;
        draw::fill_round_rect(g, static_cast<float>(h.x), static_cast<float>(h.y),
                              static_cast<float>(h.w) * std::clamp(t, 0.f, 1.f),
                              static_cast<float>(h.h), static_cast<float>(theme::radius::chip),
                              theme::color::accent, theme::alpha::slider);
      } else if (std::strcmp(h.id, "open") == 0)
        chip(h, L"Open config", false);
      else if (std::strcmp(h.id, "done") == 0)
        chip(h, L"Done", true);
    }
    (void)micro;
  }

  Config* cfg_ = nullptr;
  std::function<void(const Config&)> cb_;
  HWND hwnd_ = nullptr;
  ULONG_PTR gdiplus_token_ = 0;
  bool visible_ = false;
};

}  // namespace

std::unique_ptr<SettingsWindow> create_win_settings(Config* cfg) {
  return std::make_unique<WinSettings>(cfg);
}

}  // namespace airmouse

#endif
