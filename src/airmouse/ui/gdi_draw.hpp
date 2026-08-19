#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "airmouse/ui/theme.hpp"

namespace airmouse::draw {

inline Gdiplus::Color argb(uint32_t rgb, double a) {
  return Gdiplus::Color(static_cast<BYTE>(std::clamp(a, 0.0, 1.0) * 255.0),
                        static_cast<BYTE>((rgb >> 16) & 0xff),
                        static_cast<BYTE>((rgb >> 8) & 0xff), static_cast<BYTE>(rgb & 0xff));
}

inline void add_round_rect(Gdiplus::GraphicsPath* path, float x, float y, float w, float h,
                           float r) {
  r = std::min(r, std::min(w, h) * 0.5f);
  path->AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90);
  path->AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90);
  path->AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90);
  path->AddArc(x, y, r * 2, r * 2, 180, 90);
  path->CloseFigure();
}

inline void fill_round_rect(Gdiplus::Graphics& g, float x, float y, float w, float h, float r,
                            uint32_t rgb, double a) {
  Gdiplus::GraphicsPath path;
  add_round_rect(&path, x, y, w, h, r);
  Gdiplus::SolidBrush br(argb(rgb, a));
  g.FillPath(&br, &path);
}

inline void stroke_round_rect(Gdiplus::Graphics& g, float x, float y, float w, float h, float r,
                              uint32_t rgb, double a, float width) {
  Gdiplus::GraphicsPath path;
  add_round_rect(&path, x, y, w, h, r);
  Gdiplus::Pen pen(argb(rgb, a), width);
  g.DrawPath(&pen, &path);
}

inline void hairline(Gdiplus::Graphics& g, float x0, float y0, float x1, float y1, uint32_t rgb,
                     double a, float width) {
  Gdiplus::Pen pen(argb(rgb, a), width);
  g.DrawLine(&pen, x0, y0, x1, y1);
}

inline void dot(Gdiplus::Graphics& g, float x, float y, float r, uint32_t rgb, double a) {
  Gdiplus::SolidBrush br(argb(rgb, a));
  g.FillEllipse(&br, x - r, y - r, r * 2, r * 2);
}

inline void brackets(Gdiplus::Graphics& g, float x, float y, float w, float h, float leg,
                     uint32_t rgb, double a, float width) {
  Gdiplus::Pen pen(argb(rgb, a), width);
  const float xs[4] = {x, x + w, x, x + w};
  const float ys[4] = {y, y, y + h, y + h};
  const float dx[4] = {leg, -leg, leg, -leg};
  const float dy[4] = {leg, leg, -leg, -leg};
  for (int i = 0; i < 4; ++i) {
    g.DrawLine(&pen, xs[i] + dx[i], ys[i], xs[i], ys[i]);
    g.DrawLine(&pen, xs[i], ys[i], xs[i], ys[i] + dy[i]);
  }
}

inline void range_ticks(Gdiplus::Graphics& g, float x, float y, float w, float h, int nx, int ny,
                        uint32_t rgb, double a, float width) {
  if (nx < 2 || ny < 2) return;
  Gdiplus::Pen pen(argb(rgb, a), width);
  const float tick = 3.f;
  for (int i = 0; i < nx; ++i) {
    const float px = x + (w * i) / static_cast<float>(nx - 1);
    g.DrawLine(&pen, px, y, px, y + tick);
    g.DrawLine(&pen, px, y + h, px, y + h - tick);
  }
  for (int j = 0; j < ny; ++j) {
    const float py = y + (h * j) / static_cast<float>(ny - 1);
    g.DrawLine(&pen, x, py, x + tick, py);
    g.DrawLine(&pen, x + w, py, x + w - tick, py);
  }
}

inline void label(Gdiplus::Graphics& g, const Gdiplus::Font& font, uint32_t rgb, double a, float x,
                  float y, const wchar_t* text) {
  Gdiplus::SolidBrush br(argb(rgb, a));
  g.DrawString(text, -1, &font, Gdiplus::PointF(x, y), &br);
}

inline void segments(Gdiplus::Graphics& g, float x, float y, int filled, float scale) {
  const float sw = theme::hud::seg_w * scale;
  const float sh = theme::hud::seg_h * scale;
  const float gap = theme::hud::seg_gap * scale;
  for (int i = 0; i < theme::hud::segs; ++i) {
    const float px = x + i * (sw + gap);
    const bool on = i < filled;
    fill_round_rect(g, px, y, sw, sh, 1.f * scale, on ? theme::color::accent : theme::color::hair,
                    on ? theme::alpha::ray : theme::alpha::seg_empty);
  }
}

inline std::wstring widen(const std::string& s) {
  if (s.empty()) return L"";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

inline std::string conf_text(float confidence) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), ".%02d",
                static_cast<int>(std::lround(std::clamp(confidence, 0.f, 1.f) * 100.f)) % 100);
  if (confidence >= 0.995f) return "1.0";
  return buf;
}

inline void chip_brackets(Gdiplus::Graphics& g, const Gdiplus::Font& font, float cx, float cy,
                          const std::string& text, float scale) {
  const auto body = widen("[ " + text + " ]");
  Gdiplus::RectF bounds;
  g.MeasureString(body.c_str(), -1, &font, Gdiplus::PointF(0, 0), &bounds);
  const float pad = static_cast<float>(theme::space::sm) * scale;
  const float tw = bounds.Width + pad * 2.f;
  const float th = static_cast<float>(theme::hud::chip_h) * scale;
  const float x = cx - tw * 0.5f;
  fill_round_rect(g, x, cy, tw, th, static_cast<float>(theme::radius::tight) * scale,
                  theme::color::void_, theme::alpha::chip);
  stroke_round_rect(g, x, cy, tw, th, static_cast<float>(theme::radius::tight) * scale,
                    theme::color::accent, theme::alpha::hair, scale);
  label(g, font, theme::color::accent, theme::alpha::text, x + pad - 2.f * scale, cy + 0.5f * scale,
        body.c_str());
}

}  // namespace airmouse::draw

#endif
