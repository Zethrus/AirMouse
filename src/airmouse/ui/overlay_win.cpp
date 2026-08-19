#include "airmouse/ui/overlay.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <ole2.h>
#include <gdiplus.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "airmouse/config.hpp"
#include "airmouse/ui/gdi_draw.hpp"
#include "airmouse/ui/hud_drag.hpp"
#include "airmouse/ui/hud_metrics.hpp"
#include "airmouse/ui/theme.hpp"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "msimg32.lib")

namespace airmouse {
namespace {

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingModeSourceOver;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::PixelOffsetModeHighQuality;
using Gdiplus::PrivateFontCollection;

WorkArea win_work_area(HWND hwnd, int x, int y, int w, int h) {
  RECT probe{x, y, x + w, y + h};
  HMONITOR mon = hwnd ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
                      : MonitorFromRect(&probe, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  WorkArea work;
  if (GetMonitorInfoW(mon, &mi)) {
    work.x = static_cast<float>(mi.rcWork.left);
    work.y = static_cast<float>(mi.rcWork.top);
    work.w = static_cast<float>(mi.rcWork.right - mi.rcWork.left);
    work.h = static_cast<float>(mi.rcWork.bottom - mi.rcWork.top);
    return work;
  }
  work.w = static_cast<float>(GetSystemMetrics(SM_CXVIRTUALSCREEN));
  work.h = static_cast<float>(GetSystemMetrics(SM_CYVIRTUALSCREEN));
  work.x = static_cast<float>(GetSystemMetrics(SM_XVIRTUALSCREEN));
  work.y = static_cast<float>(GetSystemMetrics(SM_YVIRTUALSCREEN));
  return work;
}

class WinOverlay final : public Overlay {
 public:
  ~WinOverlay() override { destroy(); }

  bool create() override {
    if (hwnd_) return true;
    if (gdiplus_token_ == 0) {
      Gdiplus::GdiplusStartupInput input;
      if (Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr) != Gdiplus::Ok) {
        return false;
      }
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &WinOverlay::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseHud";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"AirMouseHud",
        L"AirMouse", WS_POPUP, 0, 0, theme::hud::w, theme::hud::h, nullptr, nullptr,
        GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    load_font();
    apply_metrics();
    drag_.place(hud_, work_);
    visible_ = true;
    dirty_ = true;
    last_tick_ = std::chrono::steady_clock::now();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    draw();
    return true;
  }

  void destroy() override {
    if (hwnd_) {
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
    font_family_.reset();
    fonts_.reset();
    if (gdiplus_token_) {
      Gdiplus::GdiplusShutdown(gdiplus_token_);
      gdiplus_token_ = 0;
    }
    visible_ = false;
  }

  void set_visible(bool on) override {
    if (!hwnd_ && on) create();
    if (!hwnd_ || visible_ == on) return;
    visible_ = on;
    ShowWindow(hwnd_, on ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (on) {
      apply_metrics();
      drag_.place(hud_, work_);
      dirty_ = true;
    }
  }

  bool visible() const override { return visible_; }

  void set_snapshot(const TrackingSnapshot& snap) override {
    if (snap_.camera_ok == snap.camera_ok && snap_.pose == snap.pose &&
        snap_.status == snap.status && snap_.message == snap.message &&
        snap_.hand.has_value() == snap.hand.has_value() &&
        std::abs(snap_.command.confidence - snap.command.confidence) <= 0.02f) {
      if (!snap.hand || (snap_.hand && std::memcmp(&snap_.hand->landmarks, &snap.hand->landmarks,
                                                   sizeof(snap.hand->landmarks)) == 0)) {
        return;
      }
    }
    snap_ = snap;
    dirty_ = true;
  }

  void set_chip(std::string text) override {
    chip_ = std::move(text);
    chip_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(900);
    dirty_ = true;
  }

  void set_placement(const HudConfig& hud) override {
    hud_ = hud;
    if (!hwnd_) return;
    apply_metrics();
    drag_.place(hud_, work_);
    dirty_ = true;
  }

  HudConfig placement() const override {
    HudConfig out = hud_;
    const HudConfig live = drag_.config();
    out.placed = live.placed;
    out.x = live.x;
    out.y = live.y;
    return out;
  }

  void set_on_moved(std::function<void(HudConfig)> cb) override { on_moved_ = std::move(cb); }

  void poll() override {
    if (hwnd_) {
      MSG msg;
      while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - last_tick_).count();
    last_tick_ = now;
    if (hwnd_ && drag_.tick(std::clamp(dt, 0.f, 0.05f), work_)) dirty_ = true;
    if (!chip_.empty() && now > chip_until_) {
      chip_.clear();
      dirty_ = true;
    }
    if (dirty_ && hwnd_ && visible_) {
      draw();
      dirty_ = false;
    }
  }

  bool wants_quit() const override { return false; }

 private:
  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WinOverlay* self = nullptr;
    if (msg == WM_NCCREATE) {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<WinOverlay*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<WinOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handle(hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  LRESULT handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_DPICHANGED) {
      const RECT* rec = reinterpret_cast<RECT*>(lparam);
      if (rec) {
        drag_.x = static_cast<float>(rec->left);
        drag_.y = static_cast<float>(rec->top);
      }
      apply_metrics();
      drag_.place(hud_, work_);
      dirty_ = true;
      return 0;
    }
    if (msg == WM_DISPLAYCHANGE) {
      apply_metrics();
      drag_.place(hud_, work_);
      dirty_ = true;
      return 0;
    }
    if (msg == WM_NCHITTEST) {
      POINT pt{GET_X_LPARAM_SAFE(lparam), GET_Y_LPARAM_SAFE(lparam)};
      ScreenToClient(hwnd, &pt);
      if (in_grip(static_cast<float>(pt.x), static_cast<float>(pt.y),
                  static_cast<float>(phys_w_), scale_)) {
        return HTCLIENT;
      }
      return HTTRANSPARENT;
    }
    if (msg == WM_SETCURSOR) {
      if (LOWORD(lparam) == HTCLIENT) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
        return TRUE;
      }
    }
    if (msg == WM_MOUSEMOVE) {
      track_leave();
      if (drag_.phase == HudDrag::Phase::Dragging) {
        POINT pt;
        GetCursorPos(&pt);
        drag_.on_move(static_cast<float>(pt.x), static_cast<float>(pt.y), work_);
        dirty_ = true;
      } else {
        drag_.on_enter();
        dirty_ = true;
      }
      return 0;
    }
    if (msg == WM_MOUSELEAVE) {
      tracking_leave_ = false;
      drag_.on_leave();
      dirty_ = true;
      return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
      POINT pt;
      GetCursorPos(&pt);
      if (drag_.on_press(static_cast<float>(pt.x), static_cast<float>(pt.y))) {
        SetCapture(hwnd);
        dirty_ = true;
      }
      return 0;
    }
    if (msg == WM_LBUTTONUP) {
      if (GetCapture() == hwnd) ReleaseCapture();
      work_ = win_work_area(hwnd_, static_cast<int>(drag_.x), static_cast<int>(drag_.y), phys_w_,
                            phys_h_);
      if (drag_.on_release(work_)) {
        hud_ = placement();
        if (on_moved_) on_moved_(hud_);
        dirty_ = true;
      }
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  static int GET_X_LPARAM_SAFE(LPARAM lp) { return static_cast<int>(static_cast<short>(LOWORD(lp))); }
  static int GET_Y_LPARAM_SAFE(LPARAM lp) { return static_cast<int>(static_cast<short>(HIWORD(lp))); }

  void track_leave() {
    if (tracking_leave_) return;
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd_;
    TrackMouseEvent(&tme);
    tracking_leave_ = true;
  }

  UINT window_dpi() const {
    if (!hwnd_) return 96;
    using Fn = UINT(WINAPI*)(HWND);
    if (HMODULE user = GetModuleHandleW(L"user32.dll")) {
      if (auto fn = reinterpret_cast<Fn>(GetProcAddress(user, "GetDpiForWindow"))) {
        const UINT dpi = fn(hwnd_);
        if (dpi) return dpi;
      }
    }
    return 96;
  }

  void apply_metrics() {
    scale_ = static_cast<float>(window_dpi()) / 96.f;
    phys_w_ = std::max(1, static_cast<int>(std::lround(theme::hud::w * scale_)));
    phys_h_ = std::max(1, static_cast<int>(std::lround(theme::hud::h * scale_)));
    drag_.set_metrics(static_cast<float>(phys_w_), static_cast<float>(phys_h_), scale_);
    work_ = win_work_area(hwnd_, static_cast<int>(drag_.x), static_cast<int>(drag_.y), phys_w_,
                          phys_h_);
  }

  void load_font() {
    fonts_ = std::make_unique<PrivateFontCollection>();
    const auto path = asset_root() / "fonts" / "IBMPlexMono-Regular.ttf";
    const auto wpath = draw::widen(path.string());
    if (!wpath.empty()) {
      fonts_->AddFontFile(wpath.c_str());
      auto fam = std::make_unique<FontFamily>(L"IBM Plex Mono", fonts_.get());
      if (fam->GetLastStatus() == Gdiplus::Ok) {
        font_family_ = std::move(fam);
      }
    }
  }

  Font make_font(float size) const {
    if (font_family_) {
      return Font(font_family_.get(), size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    }
    return Font(L"Consolas", size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  }

  const wchar_t* status_label() const {
    if (!snap_.camera_ok) return L"CAM";
    if (snap_.hand) return L"LIVE";
    return L"IDLE";
  }

  uint32_t status_color() const {
    if (!snap_.camera_ok) return theme::color::warn;
    if (snap_.hand) return theme::color::accent;
    return theme::color::dim;
  }

  void draw() {
    if (!hwnd_ || phys_w_ <= 0 || phys_h_ <= 0) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = phys_w_;
    bmi.bmiHeader.biHeight = -phys_h_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
      if (mem) DeleteDC(mem);
      if (screen) ReleaseDC(nullptr, screen);
      return;
    }
    HGDIOBJ old = SelectObject(mem, dib);
    std::memset(bits, 0, static_cast<size_t>(phys_w_) * static_cast<size_t>(phys_h_) * 4);

    {
      Bitmap bmp(phys_w_, phys_h_, phys_w_ * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
      Graphics g(&bmp);
      g.SetCompositingMode(CompositingModeSourceOver);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
      g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
      g.Clear(Color(0, 0, 0, 0));

      const HudMetrics m = hud_metrics(scale_);
      const double glass =
          theme::alpha::glass + (theme::alpha::lift - theme::alpha::glass) * drag_.lift_t;
      const double hair_a =
          theme::alpha::hair + (theme::alpha::lift_hair - theme::alpha::hair) * drag_.lift_t;
      const double grip_a =
          theme::alpha::hair_dim + (theme::alpha::hair_hot - theme::alpha::hair_dim) * drag_.hover_t;
      const double dot_a =
          theme::alpha::grip_dot + (theme::alpha::grip_dot_hot - theme::alpha::grip_dot) * drag_.hover_t;

      draw::fill_round_rect(g, m.chassis.x, m.chassis.y, m.chassis.w, m.chassis.h, m.radius,
                            theme::color::void_, glass);
      draw::stroke_round_rect(g, m.chassis.x, m.chassis.y, m.chassis.w, m.chassis.h, m.radius,
                              theme::color::hair, hair_a, scale_);
      if (drag_.lift_t > 0.01f) {
        draw::stroke_round_rect(g, m.chassis.x - scale_, m.chassis.y - scale_, m.chassis.w + 2 * scale_,
                                m.chassis.h + 2 * scale_, m.radius + scale_, theme::color::accent,
                                theme::alpha::lift_rim * drag_.lift_t, scale_);
      }
      for (const auto& p : m.dots) {
        draw::dot(g, p.x, p.y, 1.15f * scale_, theme::color::hair, dot_a);
      }
      Font micro = make_font(theme::type::micro * scale_);
      Font label = make_font(theme::type::label * scale_);
      Font caption = make_font(theme::type::caption * scale_);
      draw::label(g, micro, theme::color::paper, theme::alpha::muted, m.wordmark.x,
                  m.wordmark.y - 8.f * scale_, L"AIRMOUSE");
      draw::dot(g, m.pip.x, m.pip.y, 2.2f * scale_, status_color(), theme::alpha::text);
      draw::label(g, micro, status_color(), theme::alpha::muted, m.status.x,
                  m.status.y - 8.f * scale_, status_label());
      draw::hairline(g, 8.f * scale_, m.rule_y, m.w - 8.f * scale_, m.rule_y, theme::color::hair,
                     grip_a, scale_);
      draw::brackets(g, m.well.x, m.well.y, m.well.w, m.well.h, m.bracket, theme::color::hair,
                     theme::alpha::hair, scale_);
      draw::range_ticks(g, m.well.x, m.well.y, m.well.w, m.well.h, theme::hud::grid_x,
                        theme::hud::grid_y, theme::color::hair, theme::alpha::grid, scale_);
      draw_constellation(g, m, label);

      const std::string_view raw = snap_.status.empty() ? pose_label(snap_.pose) : snap_.status;
      draw::label(g, label, theme::color::paper, theme::alpha::text, m.pose.x,
                  m.pose.y - 12.f * scale_, draw::widen(std::string(raw)).c_str());
      const int filled = static_cast<int>(
          std::lround(std::clamp(snap_.command.confidence, 0.f, 1.f) * theme::hud::segs));
      draw::segments(g, m.seg0.x, m.seg0.y, filled, scale_);
      draw::label(g, micro, theme::color::dim, theme::alpha::muted, m.conf.x,
                  m.conf.y - 8.f * scale_,
                  draw::widen(draw::conf_text(snap_.command.confidence)).c_str());
      if (!chip_.empty()) {
        draw::chip_brackets(g, caption, m.w * 0.5f, m.chip.y, chip_, scale_);
      }
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT pt_src{0, 0};
    POINT pt_dst{static_cast<LONG>(std::lround(drag_.x)), static_cast<LONG>(std::lround(drag_.y))};
    SIZE size{phys_w_, phys_h_};
    UpdateLayeredWindow(hwnd_, screen, &pt_dst, &size, mem, &pt_src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
  }

  void draw_constellation(Graphics& g, const HudMetrics& m, const Font& font) {
    if (!snap_.hand) {
      const wchar_t* msg = snap_.camera_ok ? L"NO HAND" : L"NO CAM";
      draw::label(g, font, snap_.camera_ok ? theme::color::dim : theme::color::warn,
                  theme::alpha::empty, m.well.x + 6.f * scale_, m.well.y + m.well.h * 0.42f, msg);
      return;
    }
    const auto& lm = snap_.hand->landmarks;
    auto px = [&](int i) { return m.well.x + (1.f - lm[static_cast<size_t>(i)].x) * m.well.w; };
    auto py = [&](int i) { return m.well.y + lm[static_cast<size_t>(i)].y * m.well.h; };
    for (const auto& e : kHandBones) {
      const bool ray = index_ray(e[0], e[1]);
      draw::hairline(g, px(e[0]), py(e[0]), px(e[1]), py(e[1]),
                     ray ? theme::color::accent : theme::color::hair,
                     ray ? theme::alpha::ray : theme::alpha::bone, scale_);
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      draw::dot(g, px(i), py(i), (tip ? 2.4f : 1.4f) * scale_,
                tip ? theme::color::accent : theme::color::paper,
                tip ? theme::alpha::index_tip : theme::alpha::tip);
    }
  }

  HWND hwnd_ = nullptr;
  ULONG_PTR gdiplus_token_ = 0;
  std::unique_ptr<PrivateFontCollection> fonts_;
  std::unique_ptr<FontFamily> font_family_;
  TrackingSnapshot snap_{};
  std::string chip_;
  std::chrono::steady_clock::time_point chip_until_{};
  std::chrono::steady_clock::time_point last_tick_{};
  bool visible_ = false;
  bool dirty_ = true;
  bool tracking_leave_ = false;
  float scale_ = 1.f;
  int phys_w_ = theme::hud::w;
  int phys_h_ = theme::hud::h;
  HudConfig hud_{};
  HudDrag drag_{};
  WorkArea work_{};
  std::function<void(HudConfig)> on_moved_;
};

}  // namespace

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<WinOverlay>(); }

}  // namespace airmouse

#endif
