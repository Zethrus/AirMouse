#include "cammouse/ui/overlay.hpp"

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

#include "cammouse/config.hpp"
#include "cammouse/ui/theme.hpp"

namespace cammouse {
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
    swa.override_redirect = False;
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
    Atom notif = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(dpy_, win_, type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&notif), 1);

    XStoreName(dpy_, win_, "CamMouse");
    XClassHint hint{};
    hint.res_name = const_cast<char*>("cammouse");
    hint.res_class = const_cast<char*>("CamMouse");
    XSetClassHint(dpy_, win_, &hint);

    // Click-through: empty input shape.
    XShapeCombineRectangles(dpy_, win_, ShapeInput, 0, 0, nullptr, 0, ShapeSet,
                            Unsorted);

    surface_ = cairo_xlib_surface_create(dpy_, win_, visual_, theme::kHudW, theme::kHudH);
    cr_ = cairo_create(surface_);
    load_fonts();

    XMapRaised(dpy_, win_);
    visible_ = true;
    draw();
    return true;
  }

  void destroy() override {
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

  void draw() {
    if (!cr_) return;
    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr_, 0, 0, 0, 0);
    cairo_paint(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);

    rounded_rect(cr_, 0.5, 0.5, theme::kHudW - 1.0, theme::kHudH - 1.0, theme::kHudRadius);
    set_hex(cr_, theme::kVoid, theme::kHudAlpha);
    cairo_fill_preserve(cr_);
    set_hex(cr_, theme::kHair, 0.14);
    cairo_set_line_width(cr_, 1.0);
    cairo_stroke(cr_);

    draw_constellation();
    draw_label();
    if (!chip_.empty()) draw_chip();

    cairo_surface_flush(surface_);
    XFlush(dpy_);
  }

  void draw_constellation() {
    const double ox = 16;
    const double oy = 16;
    const double w = theme::kHudW - 32;
    const double h = 86;
    if (!snap_.hand) {
      set_hex(cr_, theme::kDim, 0.45);
      cairo_set_font_size(cr_, 10);
      cairo_select_font_face(cr_, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                             CAIRO_FONT_WEIGHT_NORMAL);
      cairo_move_to(cr_, ox + 4, oy + h * 0.55);
      cairo_show_text(cr_, snap_.camera_ok ? "NO HAND" : "NO CAM");
      return;
    }
    const auto& lm = snap_.hand->landmarks;
    auto px = [&](int i) { return ox + (1.0 - lm[static_cast<size_t>(i)].x) * w; };
    auto py = [&](int i) { return oy + lm[static_cast<size_t>(i)].y * h; };

    cairo_set_line_width(cr_, 1.0);
    cairo_set_line_cap(cr_, CAIRO_LINE_CAP_ROUND);
    for (const auto& e : kConnections) {
      const bool index_ray = (e[0] == kIndexMcp && e[1] == kIndexPip) ||
                             (e[0] == kIndexPip && e[1] == kIndexDip) ||
                             (e[0] == kIndexDip && e[1] == kIndexTip) ||
                             (e[0] == 0 && e[1] == kIndexMcp);
      set_hex(cr_, index_ray ? theme::kAccent : theme::kHair, index_ray ? 0.60 : 0.18);
      cairo_move_to(cr_, px(e[0]), py(e[0]));
      cairo_line_to(cr_, px(e[1]), py(e[1]));
      cairo_stroke(cr_);
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      set_hex(cr_, tip ? theme::kAccent : theme::kPaper, tip ? 0.90 : 0.28);
      cairo_arc(cr_, px(i), py(i), tip ? 2.4 : 1.4, 0, 6.2832);
      cairo_fill(cr_);
    }
  }

  void draw_label() {
    const char* label = pose_label(snap_.pose).data();
    set_hex(cr_, theme::kPaper, 0.92);
    cairo_select_font_face(cr_, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, 11);
    cairo_text_extents_t ext{};
    cairo_text_extents(cr_, label, &ext);
    cairo_move_to(cr_, 16, theme::kHudH - 18);
    cairo_show_text(cr_, label);

    const double tick_w = 36.0 * std::clamp(snap_.command.confidence, 0.f, 1.f);
    set_hex(cr_, theme::kAccent, 0.80);
    cairo_set_line_width(cr_, 2.0);
    cairo_move_to(cr_, theme::kHudW - 16 - 36, theme::kHudH - 20);
    cairo_line_to(cr_, theme::kHudW - 16 - 36 + tick_w, theme::kHudH - 20);
    cairo_stroke(cr_);
  }

  void draw_chip() {
    cairo_select_font_face(cr_, "IBM Plex Mono", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr_, 9);
    cairo_text_extents_t ext{};
    cairo_text_extents(cr_, chip_.c_str(), &ext);
    const double tw = ext.width + 14;
    const double th = 16;
    const double cx = (theme::kHudW - tw) * 0.5;
    const double cy = theme::kHudH - 8;
    (void)th;
    (void)cx;
    (void)cy;
    set_hex(cr_, theme::kAccent, 0.85);
    cairo_move_to(cr_, 16, theme::kHudH - 6);
    // Chip text is already the pose; keep it quiet — hairline only under the word.
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

}  // namespace cammouse

#endif
