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
#include <memory>
#include <string>
#include <vector>

#include "airmouse/config.hpp"
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
using Gdiplus::GraphicsPath;
using Gdiplus::Pen;
using Gdiplus::PixelOffsetModeHighQuality;
using Gdiplus::PointF;
using Gdiplus::PrivateFontCollection;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;

constexpr int kConnections[][2] = {
    {0, 1},  {1, 2},  {2, 3},  {3, 4},   {0, 5},  {5, 6},  {6, 7},  {7, 8},
    {0, 9},  {9, 10}, {10, 11}, {11, 12}, {0, 13}, {13, 14}, {14, 15}, {15, 16},
    {0, 17}, {17, 18}, {18, 19}, {19, 20}, {5, 9},  {9, 13}, {13, 17},
};

Color argb(uint32_t rgb, double a) {
  return Color(static_cast<BYTE>(std::clamp(a, 0.0, 1.0) * 255.0),
               static_cast<BYTE>((rgb >> 16) & 0xff), static_cast<BYTE>((rgb >> 8) & 0xff),
               static_cast<BYTE>(rgb & 0xff));
}

std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

void add_round_rect(GraphicsPath* path, float x, float y, float w, float h, float r) {
  r = std::min(r, std::min(w, h) * 0.5f);
  path->AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
  path->AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
  path->AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
  path->AddArc(x, y, r * 2, r * 2, 180, 90);
  path->CloseFigure();
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
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"AirMouseHud", L"AirMouse", WS_POPUP, 0, 0, theme::kHudW, theme::kHudH, nullptr, nullptr,
        GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    load_font();
    apply_layout();
    visible_ = true;
    dirty_ = true;
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
      apply_layout();
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

  void poll() override {
    if (hwnd_) {
      MSG msg;
      while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    const auto now = std::chrono::steady_clock::now();
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
    (void)wparam;
    if (msg == WM_DPICHANGED) {
      const RECT* rec = reinterpret_cast<RECT*>(lparam);
      if (rec) {
        x_ = rec->left;
        y_ = rec->top;
      }
      apply_layout();
      dirty_ = true;
      return 0;
    }
    if (msg == WM_DISPLAYCHANGE) {
      apply_layout();
      dirty_ = true;
      return 0;
    }
    if (msg == WM_NCHITTEST) {
      return HTTRANSPARENT;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
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

  void apply_layout() {
    if (!hwnd_) return;
    scale_ = static_cast<float>(window_dpi()) / 96.f;
    phys_w_ = std::max(1, static_cast<int>(std::lround(theme::kHudW * scale_)));
    phys_h_ = std::max(1, static_cast<int>(std::lround(theme::kHudH * scale_)));

    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi)) {
      const int pad = static_cast<int>(std::lround(24.f * scale_));
      x_ = mi.rcWork.right - phys_w_ - pad;
      y_ = mi.rcWork.top + pad;
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, x_, y_, phys_w_, phys_h_, SWP_NOACTIVATE);
  }

  void load_font() {
    fonts_ = std::make_unique<PrivateFontCollection>();
    const auto path = asset_root() / "fonts" / "IBMPlexMono-Regular.ttf";
    const auto wpath = widen(path.string());
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

      GraphicsPath panel;
      add_round_rect(&panel, 0.5f * scale_, 0.5f * scale_,
                     static_cast<float>(phys_w_) - scale_, static_cast<float>(phys_h_) - scale_,
                     static_cast<float>(theme::kHudRadius) * scale_);
      SolidBrush fill(argb(theme::kVoid, theme::kHudAlpha));
      g.FillPath(&fill, &panel);
      Pen hair(argb(theme::kHair, 0.14), scale_);
      g.DrawPath(&hair, &panel);

      draw_constellation(g);
      draw_label(g);
      if (!chip_.empty()) draw_chip(g);
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT pt_src{0, 0};
    POINT pt_dst{x_, y_};
    SIZE size{phys_w_, phys_h_};
    UpdateLayeredWindow(hwnd_, screen, &pt_dst, &size, mem, &pt_src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
  }

  void draw_constellation(Graphics& g) {
    const float ox = 16.f * scale_;
    const float oy = 16.f * scale_;
    const float w = static_cast<float>(phys_w_) - 32.f * scale_;
    const float h = 86.f * scale_;
    if (!snap_.hand) {
      SolidBrush dim(argb(theme::kDim, 0.45));
      Font font = make_font(10.f * scale_);
      const wchar_t* msg = snap_.camera_ok ? L"NO HAND" : L"NO CAM";
      g.DrawString(msg, -1, &font, PointF(ox + 4.f * scale_, oy + h * 0.42f), &dim);
      return;
    }
    const auto& lm = snap_.hand->landmarks;
    auto px = [&](int i) { return ox + (1.f - lm[static_cast<size_t>(i)].x) * w; };
    auto py = [&](int i) { return oy + lm[static_cast<size_t>(i)].y * h; };

    for (const auto& e : kConnections) {
      const bool index_ray = (e[0] == kIndexMcp && e[1] == kIndexPip) ||
                             (e[0] == kIndexPip && e[1] == kIndexDip) ||
                             (e[0] == kIndexDip && e[1] == kIndexTip) ||
                             (e[0] == 0 && e[1] == kIndexMcp);
      Pen pen(argb(index_ray ? theme::kAccent : theme::kHair, index_ray ? 0.60 : 0.18), scale_);
      g.DrawLine(&pen, px(e[0]), py(e[0]), px(e[1]), py(e[1]));
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      SolidBrush br(argb(tip ? theme::kAccent : theme::kPaper, tip ? 0.90 : 0.28));
      const float r = (tip ? 2.4f : 1.4f) * scale_;
      g.FillEllipse(&br, px(i) - r, py(i) - r, r * 2, r * 2);
    }
  }

  void draw_label(Graphics& g) {
    Font font = make_font(11.f * scale_);
    SolidBrush paper(argb(theme::kPaper, 0.92));
    const std::string_view raw =
        snap_.status.empty() ? pose_label(snap_.pose) : snap_.status;
    const auto label = widen(std::string(raw));
    g.DrawString(label.c_str(), -1, &font,
                 PointF(16.f * scale_, static_cast<float>(phys_h_) - 28.f * scale_), &paper);
    const float tick = 36.f * scale_ * std::clamp(snap_.command.confidence, 0.f, 1.f);
    Pen accent(argb(theme::kAccent, 0.80), 2.f * scale_);
    const float x0 = static_cast<float>(phys_w_) - 16.f * scale_ - 36.f * scale_;
    const float y0 = static_cast<float>(phys_h_) - 20.f * scale_;
    g.DrawLine(&accent, x0, y0, x0 + tick, y0);
  }

  void draw_chip(Graphics& g) {
    Font font = make_font(9.f * scale_);
    const auto text = widen(chip_);
    RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, PointF(0, 0), &bounds);
    const float pad = 7.f * scale_;
    const float tw = bounds.Width + pad * 2.f;
    const float th = 16.f * scale_;
    const float cx = (static_cast<float>(phys_w_) - tw) * 0.5f;
    const float cy = 6.f * scale_;
    GraphicsPath pill;
    add_round_rect(&pill, cx, cy, tw, th, th * 0.5f);
    SolidBrush fill(argb(theme::kAccent, 0.90));
    g.FillPath(&fill, &pill);
    SolidBrush ink(argb(theme::kVoid, 0.95));
    g.DrawString(text.c_str(), -1, &font, PointF(cx + pad - 2.f * scale_, cy + 0.5f * scale_),
                 &ink);
  }

  HWND hwnd_ = nullptr;
  ULONG_PTR gdiplus_token_ = 0;
  std::unique_ptr<PrivateFontCollection> fonts_;
  std::unique_ptr<FontFamily> font_family_;
  TrackingSnapshot snap_{};
  std::string chip_;
  std::chrono::steady_clock::time_point chip_until_{};
  bool visible_ = false;
  bool dirty_ = true;
  float scale_ = 1.f;
  int phys_w_ = theme::kHudW;
  int phys_h_ = theme::kHudH;
  int x_ = 0;
  int y_ = 0;
};

}  // namespace

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<WinOverlay>(); }

}  // namespace airmouse

#endif
