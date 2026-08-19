#pragma once

#ifndef _WIN32

#include <cairo/cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "airmouse/ui/theme.hpp"

namespace airmouse::draw {

inline void set_hex(cairo_t* cr, uint32_t rgb, double a) {
  cairo_set_source_rgba(cr, ((rgb >> 16) & 0xff) / 255.0, ((rgb >> 8) & 0xff) / 255.0,
                        (rgb & 0xff) / 255.0, a);
}

inline void round_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
  r = std::min(r, std::min(w, h) * 0.5);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -1.5708, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, 1.5708);
  cairo_arc(cr, x + r, y + h - r, r, 1.5708, 3.1416);
  cairo_arc(cr, x + r, y + r, r, 3.1416, 4.7124);
  cairo_close_path(cr);
}

inline void fill_round_rect(cairo_t* cr, double x, double y, double w, double h, double r,
                            uint32_t rgb, double a) {
  round_rect(cr, x, y, w, h, r);
  set_hex(cr, rgb, a);
  cairo_fill(cr);
}

inline void stroke_round_rect(cairo_t* cr, double x, double y, double w, double h, double r,
                              uint32_t rgb, double a, double width) {
  round_rect(cr, x, y, w, h, r);
  set_hex(cr, rgb, a);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
}

inline void hairline(cairo_t* cr, double x0, double y0, double x1, double y1, uint32_t rgb, double a,
                     double width) {
  set_hex(cr, rgb, a);
  cairo_set_line_width(cr, width);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
  cairo_stroke(cr);
}

inline void dot(cairo_t* cr, double x, double y, double r, uint32_t rgb, double a) {
  set_hex(cr, rgb, a);
  cairo_arc(cr, x, y, r, 0, 6.2832);
  cairo_fill(cr);
}

inline void brackets(cairo_t* cr, double x, double y, double w, double h, double leg, uint32_t rgb,
                     double a, double width) {
  set_hex(cr, rgb, a);
  cairo_set_line_width(cr, width);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
  const double xs[4] = {x, x + w, x, x + w};
  const double ys[4] = {y, y, y + h, y + h};
  const double dx[4] = {leg, -leg, leg, -leg};
  const double dy[4] = {leg, leg, -leg, -leg};
  for (int i = 0; i < 4; ++i) {
    cairo_move_to(cr, xs[i] + dx[i], ys[i]);
    cairo_line_to(cr, xs[i], ys[i]);
    cairo_line_to(cr, xs[i], ys[i] + dy[i]);
    cairo_stroke(cr);
  }
}

inline void range_ticks(cairo_t* cr, double x, double y, double w, double h, int nx, int ny,
                        uint32_t rgb, double a, double width) {
  if (nx < 2 || ny < 2) return;
  set_hex(cr, rgb, a);
  cairo_set_line_width(cr, width);
  const double tick = 3.0;
  for (int i = 0; i < nx; ++i) {
    const double px = x + (w * i) / (nx - 1);
    cairo_move_to(cr, px, y);
    cairo_line_to(cr, px, y + tick);
    cairo_move_to(cr, px, y + h);
    cairo_line_to(cr, px, y + h - tick);
  }
  for (int j = 0; j < ny; ++j) {
    const double py = y + (h * j) / (ny - 1);
    cairo_move_to(cr, x, py);
    cairo_line_to(cr, x + tick, py);
    cairo_move_to(cr, x + w, py);
    cairo_line_to(cr, x + w - tick, py);
  }
  cairo_stroke(cr);
}

inline void label(cairo_t* cr, const char* face, double size, uint32_t rgb, double a, double x,
                  double y, const char* text) {
  cairo_select_font_face(cr, face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, size);
  set_hex(cr, rgb, a);
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);
}

inline void label_mono(cairo_t* cr, double size, uint32_t rgb, double a, double x, double y,
                       const char* text) {
  label(cr, theme::font::mono, size, rgb, a, x, y, text);
}

inline void label_sans(cairo_t* cr, double size, uint32_t rgb, double a, double x, double y,
                       const char* text) {
  label(cr, theme::font::sans, size, rgb, a, x, y, text);
}

inline void segments(cairo_t* cr, double x, double y, int filled, float scale) {
  const double sw = theme::hud::seg_w * scale;
  const double sh = theme::hud::seg_h * scale;
  const double gap = theme::hud::seg_gap * scale;
  for (int i = 0; i < theme::hud::segs; ++i) {
    const double px = x + i * (sw + gap);
    const bool on = i < filled;
    fill_round_rect(cr, px, y, sw, sh, 1.0 * scale, on ? theme::color::accent : theme::color::hair,
                    on ? theme::alpha::ray : theme::alpha::seg_empty);
  }
}

inline void chip_brackets(cairo_t* cr, double cx, double cy, const char* text, float scale) {
  std::string body = "[ ";
  body += text;
  body += " ]";
  cairo_select_font_face(cr, theme::font::mono, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, theme::type::caption * scale);
  cairo_text_extents_t ext{};
  cairo_text_extents(cr, body.c_str(), &ext);
  const double pad = theme::space::sm * scale;
  const double tw = ext.width + pad * 2;
  const double th = theme::hud::chip_h * scale;
  const double x = cx - tw * 0.5;
  fill_round_rect(cr, x, cy, tw, th, theme::radius::tight * scale, theme::color::void_,
                  theme::alpha::chip);
  stroke_round_rect(cr, x, cy, tw, th, theme::radius::tight * scale, theme::color::accent,
                    theme::alpha::hair, 1.0 * scale);
  set_hex(cr, theme::color::accent, theme::alpha::text);
  cairo_move_to(cr, x + pad - ext.x_bearing, cy + (th + ext.height) * 0.5);
  cairo_show_text(cr, body.c_str());
}

inline std::string conf_text(float confidence) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), ".%02d",
                static_cast<int>(std::lround(std::clamp(confidence, 0.f, 1.f) * 100.f)) % 100);
  if (confidence >= 0.995f) return "1.0";
  return buf;
}

}  // namespace airmouse::draw

#endif
