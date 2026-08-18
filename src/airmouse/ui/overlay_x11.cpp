#include "airmouse/ui/overlay.hpp"

#ifndef _WIN32

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

#include "airmouse/config.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

constexpr int kConnections[][2] = {
    {0, 1},  {1, 2},  {2, 3},  {3, 4},   {0, 5},  {5, 6},  {6, 7},  {7, 8},
    {0, 9},  {9, 10}, {10, 11}, {11, 12}, {0, 13}, {13, 14}, {14, 15}, {15, 16},
    {0, 17}, {17, 18}, {18, 19}, {19, 20}, {5, 9},  {9, 13}, {13, 17},
};

void rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -1.5708, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, 1.5708);
  cairo_arc(cr, x + r, y + h - r, r, 1.5708, 3.1416);
  cairo_arc(cr, x + r, y + r, r, 3.1416, 4.7124);
  cairo_close_path(cr);
}

void set_hex(cairo_t* cr, uint32_t rgb, double a) {
  cairo_set_source_rgba(cr, ((rgb >> 16) & 0xff) / 255.0, ((rgb >> 8) & 0xff) / 255.0,
                        (rgb & 0xff) / 255.0, a);
}

class X11Overlay final : public Overlay {
 public:
  ~X11Overlay() override { destroy(); }

  bool create() override {
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) return false;
    screen_ = DefaultScreen(dpy_);
    root_ = RootWindow(dpy_, screen_);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy_, screen_, 32, TrueColor, &vinfo)) {
      if (!XMatchVisualInfo(dpy_, screen_, 24, TrueColor, &vinfo)) {
        return false;
      }
    }
    visual_ = vinfo.visual;
    depth_ = vinfo.depth;

    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy_, root_, visual_, AllocNone);
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.override_redirect = True;
    swa.event_mask = ExposureMask | StructureNotifyMask;

    const int sw = DisplayWidth(dpy_, screen_);
    x_ = sw - theme::kHudW - 24;
    y_ = 24;

    win_ = XCreateWindow(dpy_, root_, x_, y_, theme::kHudW, theme::kHudH, 0, depth_,
                         InputOutput, visual_,
                         CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect |
                             CWEventMask,
                         &swa);

    Atom net_state = XInternAtom(dpy_, "_NET_WM_STATE", False);
    Atom above = XInternAtom(dpy_, "_NET_WM_STATE_ABOVE", False);
    Atom skip_task = XInternAtom(dpy_, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(dpy_, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom states[] = {above, skip_task, skip_pager};
    XChangeProperty(dpy_, win_, net_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states), 3);

    Atom type = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE", False);
    Atom utility = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    XChangeProperty(dpy_, win_, type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&utility), 1);

    XStoreName(dpy_, win_, "AirMouse");
    XClassHint hint{};
    hint.res_name = const_cast<char*>("airmouse");
    hint.res_class = const_cast<char*>("AirMouse");
    XSetClassHint(dpy_, win_, &hint);

    // Click-through: empty input shape.
    XShapeCombineRectangles(dpy_, win_, ShapeInput, 0, 0, nullptr, 0, ShapeSet,
                            Unsorted);

    surface_ = cairo_xlib_surface_create(dpy_, win_, visual_, theme::kHudW, theme::kHudH);
    cr_ = cairo_create(surface_);
    offscreen_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, theme::kHudW, theme::kHudH);
    off_cr_ = cairo_create(offscreen_);
    load_fonts();

    XMapRaised(dpy_, win_);
    visible_ = true;
    draw();
    return true;
  }

  void destroy() override {
    if (off_cr_) cairo_destroy(off_cr_);
    if (offscreen_) cairo_surface_destroy(offscreen_);
    off_cr_ = nullptr;
    offscreen_ = nullptr;
    if (cr_) cairo_destroy(cr_);
    if (surface_) cairo_surface_destroy(surface_);
    cr_ = nullptr;
    surface_ = nullptr;
    if (dpy_ && win_) XDestroyWindow(dpy_, win_);
    if (dpy_) XCloseDisplay(dpy_);
    dpy_ = nullptr;
    win_ = 0;
  }

  void set_visible(bool on) override {
    if (!dpy_ || !win_ || visible_ == on) return;
    visible_ = on;
    if (on) {
      XMapRaised(dpy_, win_);
    } else {
      XUnmapWindow(dpy_, win_);
    }
    XFlush(dpy_);
  }

  bool visible() const override { return visible_; }

  void set_snapshot(const TrackingSnapshot& snap) override {
    if (same_snap(snap_, snap)) return;
    snap_ = snap;
    dirty_ = true;
  }

  void set_chip(std::string text) override {
    chip_ = std::move(text);
    chip_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(900);
    dirty_ = true;
  }

  void poll() override {
    if (!dpy_) return;
    while (XPending(dpy_)) {
      XEvent ev;
      XNextEvent(dpy_, &ev);
      if (ev.type == Expose) dirty_ = true;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!chip_.empty() && now > chip_until_) {
      chip_.clear();
      dirty_ = true;
    }
    if (dirty_) {
      draw();
      dirty_ = false;
    }
  }

  bool wants_quit() const override { return false; }

 private:
  void load_fonts() {
    const auto fonts = asset_root() / "fonts";
    font_mono_ = (fonts / "IBMPlexMono-Regular.ttf").string();
    font_sans_ = (fonts / "IBMPlexSans-Regular.ttf").string();
  }

  static bool same_snap(const TrackingSnapshot& a, const TrackingSnapshot& b) {
    if (a.camera_ok != b.camera_ok || a.pose != b.pose || a.status != b.status ||
        a.message != b.message || a.hand.has_value() != b.hand.has_value()) {
      return false;
    }
    if (std::abs(a.command.confidence - b.command.confidence) > 0.02f) return false;
    if (a.hand && b.hand) {
      return std::memcmp(&a.hand->landmarks, &b.hand->landmarks, sizeof(a.hand->landmarks)) == 0;
    }
    return true;
  }

  void draw() {
    if (!cr_ || !off_cr_) return;
    cairo_t* cr = off_cr_;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    rounded_rect(cr, 0.5, 0.5, theme::kHudW - 1.0, theme::kHudH - 1.0, theme::kHudRadius);
    set_hex(cr, theme::kVoid, theme::kHudAlpha);
    cairo_fill_preserve(cr);
    set_hex(cr, theme::kHair, 0.14);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    draw_constellation(cr);
    draw_label(cr);
    if (!chip_.empty()) draw_chip(cr);

    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr_, offscreen_, 0, 0);
    cairo_paint(cr_);
    cairo_surface_flush(surface_);
    XFlush(dpy_);
  }

  void draw_constellation(cairo_t* cr) {
    const double ox = 16;
    const double oy = 16;
    const double w = theme::kHudW - 32;
    const double h = 86;
    if (!snap_.hand) {
      set_hex(cr, theme::kDim, 0.45);
      cairo_set_font_size(cr, 10);
      cairo_select_font_face(cr, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                             CAIRO_FONT_WEIGHT_NORMAL);
      cairo_move_to(cr, ox + 4, oy + h * 0.42);
      cairo_show_text(cr, snap_.camera_ok ? "NO HAND" : "NO CAM");
      if (!snap_.message.empty()) {
        cairo_set_font_size(cr, 8);
        cairo_move_to(cr, ox + 4, oy + h * 0.62);
        const std::string clipped = snap_.message.size() > 34
                                        ? snap_.message.substr(0, 31) + "..."
                                        : snap_.message;
        cairo_show_text(cr, clipped.c_str());
      }
      return;
    }
    const auto& lm = snap_.hand->landmarks;
    auto px = [&](int i) { return ox + (1.0 - lm[static_cast<size_t>(i)].x) * w; };
    auto py = [&](int i) { return oy + lm[static_cast<size_t>(i)].y * h; };

    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (const auto& e : kConnections) {
      const bool index_ray = (e[0] == kIndexMcp && e[1] == kIndexPip) ||
                             (e[0] == kIndexPip && e[1] == kIndexDip) ||
                             (e[0] == kIndexDip && e[1] == kIndexTip) ||
                             (e[0] == 0 && e[1] == kIndexMcp);
      set_hex(cr, index_ray ? theme::kAccent : theme::kHair, index_ray ? 0.60 : 0.18);
      cairo_move_to(cr, px(e[0]), py(e[0]));
      cairo_line_to(cr, px(e[1]), py(e[1]));
      cairo_stroke(cr);
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      set_hex(cr, tip ? theme::kAccent : theme::kPaper, tip ? 0.90 : 0.28);
      cairo_arc(cr, px(i), py(i), tip ? 2.4 : 1.4, 0, 6.2832);
      cairo_fill(cr);
    }
  }

  void draw_label(cairo_t* cr) {
    const char* label =
        snap_.status.empty() ? pose_label(snap_.pose).data() : snap_.status.data();
    set_hex(cr, theme::kPaper, 0.92);
    cairo_select_font_face(cr, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    cairo_text_extents_t ext{};
    cairo_text_extents(cr, label, &ext);
    cairo_move_to(cr, 16, theme::kHudH - 18);
    cairo_show_text(cr, label);

    const double tick_w = 36.0 * std::clamp(snap_.command.confidence, 0.f, 1.f);
    set_hex(cr, theme::kAccent, 0.80);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, theme::kHudW - 16 - 36, theme::kHudH - 20);
    cairo_line_to(cr, theme::kHudW - 16 - 36 + tick_w, theme::kHudH - 20);
    cairo_stroke(cr);
  }

  void draw_chip(cairo_t* cr) {
    cairo_select_font_face(cr, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9);
    cairo_text_extents_t ext{};
    cairo_text_extents(cr, chip_.c_str(), &ext);
    const double pad = 7;
    const double tw = ext.width + pad * 2;
    const double th = 15;
    const double cx = (theme::kHudW - tw) * 0.5;
    const double cy = 6;
    rounded_rect(cr, cx, cy, tw, th, th * 0.5);
    set_hex(cr, theme::kAccent, 0.90);
    cairo_fill(cr);
    set_hex(cr, theme::kVoid, 0.95);
    cairo_move_to(cr, cx + pad - ext.x_bearing, cy + (th + ext.height) * 0.5);
    cairo_show_text(cr, chip_.c_str());
  }

  Display* dpy_ = nullptr;
  int screen_ = 0;
  Window root_ = 0;
  Window win_ = 0;
  Visual* visual_ = nullptr;
  int depth_ = 32;
  int x_ = 0;
  int y_ = 0;
  cairo_surface_t* surface_ = nullptr;
  cairo_t* cr_ = nullptr;
  cairo_surface_t* offscreen_ = nullptr;
  cairo_t* off_cr_ = nullptr;
  TrackingSnapshot snap_{};
  std::string chip_;
  std::chrono::steady_clock::time_point chip_until_{};
  bool visible_ = false;
  bool dirty_ = true;
  std::string font_mono_;
  std::string font_sans_;
};

}  // namespace

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<X11Overlay>(); }

}  // namespace airmouse

#endif
