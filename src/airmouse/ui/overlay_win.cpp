#include "airmouse/ui/overlay.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::PrivateFontCollection;
using Gdiplus::SolidBrush;
using Gdiplus::Status;

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
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"AirMouseHud";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    x_ = sw - theme::kHudW - 24;
    y_ = 24;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"AirMouseHud", L"AirMouse", WS_POPUP, x_, y_, theme::kHudW, theme::kHudH, nullptr,
        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return false;

    load_font();
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
  }

  bool visible() const override { return visible_; }

  void set_snapshot(const TrackingSnapshot& snap) override {
    snap_ = snap;
    dirty_ = true;
  }

  void set_chip(std::string text) override {
    chip_ = std::move(text);
    chip_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(900);
    dirty_ = true;
  }

  void poll() override {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
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
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = theme::kHudW;
    bmi.bmiHeader.biHeight = -theme::kHudH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, dib);

    {
      Graphics g(mem);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
      g.Clear(Color(0, 0, 0, 0));

      GraphicsPath panel;
      add_round_rect(&panel, 0.5f, 0.5f, static_cast<float>(theme::kHudW - 1),
                     static_cast<float>(theme::kHudH - 1),
                     static_cast<float>(theme::kHudRadius));
      SolidBrush fill(argb(theme::kVoid, theme::kHudAlpha));
      g.FillPath(&fill, &panel);
      Pen hair(argb(theme::kHair, 0.14), 1.0f);
      g.DrawPath(&hair, &panel);

      draw_constellation(g);
      draw_label(g);
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT pt_src{0, 0};
    POINT pt_dst{x_, y_};
    SIZE size{theme::kHudW, theme::kHudH};
    UpdateLayeredWindow(hwnd_, screen, &pt_dst, &size, mem, &pt_src, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
  }

  void draw_constellation(Graphics& g) {
    const float ox = 16;
    const float oy = 16;
    const float w = static_cast<float>(theme::kHudW - 32);
    const float h = 86;
    if (!snap_.hand) {
      SolidBrush dim(argb(theme::kDim, 0.45));
      Font font = make_font(10);
      const wchar_t* msg = snap_.camera_ok ? L"NO HAND" : L"NO CAM";
      g.DrawString(msg, -1, &font, PointF(ox + 4, oy + h * 0.45f), &dim);
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
      Pen pen(argb(index_ray ? theme::kAccent : theme::kHair, index_ray ? 0.60 : 0.18), 1.0f);
      g.DrawLine(&pen, px(e[0]), py(e[0]), px(e[1]), py(e[1]));
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      SolidBrush br(argb(tip ? theme::kAccent : theme::kPaper, tip ? 0.90 : 0.28));
      const float r = tip ? 2.4f : 1.4f;
      g.FillEllipse(&br, px(i) - r, py(i) - r, r * 2, r * 2);
    }
  }

  void draw_label(Graphics& g) {
    Font font = make_font(11);
    SolidBrush paper(argb(theme::kPaper, 0.92));
    const auto label = widen(std::string(pose_label(snap_.pose)));
    g.DrawString(label.c_str(), -1, &font, PointF(16, static_cast<float>(theme::kHudH - 28)),
                 &paper);
    const float tick = 36.f * std::clamp(snap_.command.confidence, 0.f, 1.f);
    Pen accent(argb(theme::kAccent, 0.80), 2.0f);
    const float x0 = static_cast<float>(theme::kHudW - 16 - 36);
    const float y0 = static_cast<float>(theme::kHudH - 20);
    g.DrawLine(&accent, x0, y0, x0 + tick, y0);
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
  int x_ = 0;
  int y_ = 0;
};

}  // namespace

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<WinOverlay>(); }

}  // namespace airmouse

#endif
