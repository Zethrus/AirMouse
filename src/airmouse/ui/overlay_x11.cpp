#include "airmouse/ui/overlay.hpp"

#ifndef _WIN32

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>

#include "airmouse/config.hpp"
#include "airmouse/ui/cairo_draw.hpp"
#include "airmouse/ui/hud_drag.hpp"
#include "airmouse/ui/hud_metrics.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

WorkArea x11_work_area(Display* dpy, int screen, Window root) {
  WorkArea work;
  work.w = static_cast<float>(DisplayWidth(dpy, screen));
  work.h = static_cast<float>(DisplayHeight(dpy, screen));
  Atom net = XInternAtom(dpy, "_NET_WORKAREA", True);
  if (net == None) return work;
  Atom actual = None;
  int format = 0;
  unsigned long nitems = 0;
  unsigned long bytes = 0;
  unsigned char* prop = nullptr;
  if (XGetWindowProperty(dpy, root, net, 0, 4, False, XA_CARDINAL, &actual, &format, &nitems,
                         &bytes, &prop) == Success &&
      prop && nitems >= 4 && format == 32) {
    const auto* v = reinterpret_cast<long*>(prop);
    work.x = static_cast<float>(v[0]);
    work.y = static_cast<float>(v[1]);
    work.w = static_cast<float>(v[2]);
    work.h = static_cast<float>(v[3]);
  }
  if (prop) XFree(prop);
  return work;
}

class X11Overlay final : public Overlay {
 public:
  ~X11Overlay() override { destroy(); }

  bool create() override {
    if (dpy_) return true;
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

    work_ = x11_work_area(dpy_, screen_, root_);
    drag_.set_metrics(static_cast<float>(theme::hud::w), static_cast<float>(theme::hud::h), 1.f);
    drag_.place(hud_, work_);

    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy_, root_, visual_, AllocNone);
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.override_redirect = True;
    swa.event_mask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | EnterWindowMask | LeaveWindowMask | OwnerGrabButtonMask;

    win_ = XCreateWindow(dpy_, root_, static_cast<int>(std::lround(drag_.x)),
                         static_cast<int>(std::lround(drag_.y)), theme::hud::w, theme::hud::h, 0,
                         depth_, InputOutput, visual_,
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

    apply_input_shape();
    grab_cursor_ = XCreateFontCursor(dpy_, XC_fleur);
    arrow_cursor_ = XCreateFontCursor(dpy_, XC_left_ptr);

    surface_ = cairo_xlib_surface_create(dpy_, win_, visual_, theme::hud::w, theme::hud::h);
    cr_ = cairo_create(surface_);
    offscreen_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, theme::hud::w, theme::hud::h);
    off_cr_ = cairo_create(offscreen_);
    last_tick_ = std::chrono::steady_clock::now();

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
    if (dpy_ && grab_cursor_) XFreeCursor(dpy_, grab_cursor_);
    if (dpy_ && arrow_cursor_) XFreeCursor(dpy_, arrow_cursor_);
    grab_cursor_ = 0;
    arrow_cursor_ = 0;
    if (dpy_ && win_) XDestroyWindow(dpy_, win_);
    if (dpy_) XCloseDisplay(dpy_);
    dpy_ = nullptr;
    win_ = 0;
  }

  void set_visible(bool on) override {
    if (!dpy_ || !win_ || visible_ == on) return;
    visible_ = on;
    if (on) {
      work_ = x11_work_area(dpy_, screen_, root_);
      drag_.place(hud_, work_);
      sync_window_pos();
      XMapRaised(dpy_, win_);
      dirty_ = true;
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

  void set_placement(const HudConfig& hud) override {
    hud_ = hud;
    if (!dpy_) return;
    work_ = x11_work_area(dpy_, screen_, root_);
    drag_.place(hud_, work_);
    sync_window_pos();
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
    if (!dpy_) return;
    while (XPending(dpy_)) {
      XEvent ev;
      XNextEvent(dpy_, &ev);
      handle_event(ev);
    }
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - last_tick_).count();
    last_tick_ = now;
    if (drag_.tick(std::clamp(dt, 0.f, 0.05f), work_)) {
      sync_window_pos();
      dirty_ = true;
    }
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
  void apply_input_shape() {
    XRectangle r{};
    r.x = 0;
    r.y = 0;
    r.width = static_cast<unsigned short>(theme::hud::w);
    r.height = static_cast<unsigned short>(theme::hud::grip_h);
    XShapeCombineRectangles(dpy_, win_, ShapeInput, 0, 0, &r, 1, ShapeSet, Unsorted);
  }

  void sync_window_pos() {
    if (!dpy_ || !win_) return;
    const int nx = static_cast<int>(std::lround(drag_.x));
    const int ny = static_cast<int>(std::lround(drag_.y));
    if (nx == x_ && ny == y_) return;
    x_ = nx;
    y_ = ny;
    XMoveWindow(dpy_, win_, x_, y_);
  }

  void persist() {
    hud_ = placement();
    if (on_moved_) on_moved_(hud_);
  }

  void apply_cursor() {
    if (!dpy_ || !win_) return;
    const bool grab = drag_.phase == HudDrag::Phase::Hover || drag_.phase == HudDrag::Phase::Dragging ||
                      drag_.phase == HudDrag::Phase::Settling;
    XDefineCursor(dpy_, win_, grab ? grab_cursor_ : arrow_cursor_);
  }

  void handle_event(const XEvent& ev) {
    if (ev.type == Expose) {
      dirty_ = true;
      return;
    }
    if (ev.type == EnterNotify) {
      drag_.on_enter();
      apply_cursor();
      dirty_ = true;
      return;
    }
    if (ev.type == LeaveNotify) {
      drag_.on_leave();
      apply_cursor();
      dirty_ = true;
      return;
    }
    if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
      if (drag_.on_press(static_cast<float>(ev.xbutton.x_root),
                         static_cast<float>(ev.xbutton.y_root))) {
        XGrabPointer(dpy_, win_, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, grab_cursor_, CurrentTime);
        grabbed_ = true;
        dirty_ = true;
      }
      return;
    }
    if (ev.type == MotionNotify) {
      if (drag_.phase == HudDrag::Phase::Dragging) {
        drag_.on_move(static_cast<float>(ev.xmotion.x_root), static_cast<float>(ev.xmotion.y_root),
                      work_);
        sync_window_pos();
        dirty_ = true;
      }
      return;
    }
    if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
      if (grabbed_) {
        XUngrabPointer(dpy_, CurrentTime);
        grabbed_ = false;
      }
      if (drag_.on_release(work_)) {
        persist();
        apply_cursor();
        dirty_ = true;
      }
    }
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

  const char* status_label() const {
    if (!snap_.camera_ok) return "CAM";
    if (snap_.hand) return "LIVE";
    return "IDLE";
  }

  uint32_t status_color() const {
    if (!snap_.camera_ok) return theme::color::warn;
    if (snap_.hand) return theme::color::accent;
    return theme::color::dim;
  }

  void draw() {
    if (!cr_ || !off_cr_) return;
    cairo_t* cr = off_cr_;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    const HudMetrics m = hud_metrics(1.f);
    const double glass = theme::alpha::glass + (theme::alpha::lift - theme::alpha::glass) * drag_.lift_t;
    const double hair_a =
        theme::alpha::hair + (theme::alpha::lift_hair - theme::alpha::hair) * drag_.lift_t;
    const double grip_a =
        theme::alpha::hair_dim + (theme::alpha::hair_hot - theme::alpha::hair_dim) * drag_.hover_t;
    const double dot_a =
        theme::alpha::grip_dot + (theme::alpha::grip_dot_hot - theme::alpha::grip_dot) * drag_.hover_t;

    draw::fill_round_rect(cr, m.chassis.x, m.chassis.y, m.chassis.w, m.chassis.h, m.radius,
                          theme::color::void_, glass);
    draw::stroke_round_rect(cr, m.chassis.x, m.chassis.y, m.chassis.w, m.chassis.h, m.radius,
                            theme::color::hair, hair_a, 1.0);
    if (drag_.lift_t > 0.01f) {
      draw::stroke_round_rect(cr, m.chassis.x - 1, m.chassis.y - 1, m.chassis.w + 2,
                              m.chassis.h + 2, m.radius + 1, theme::color::accent,
                              theme::alpha::lift_rim * drag_.lift_t, 1.0);
    }

    for (const auto& p : m.dots) {
      draw::dot(cr, p.x, p.y, 1.15, theme::color::hair, dot_a);
    }
    draw::label_mono(cr, theme::type::micro, theme::color::paper, theme::alpha::muted, m.wordmark.x,
                     m.wordmark.y, "AIRMOUSE");
    draw::dot(cr, m.pip.x, m.pip.y, 2.2, status_color(), theme::alpha::text);
    draw::label_mono(cr, theme::type::micro, status_color(), theme::alpha::muted, m.status.x,
                     m.status.y, status_label());
    draw::hairline(cr, 8, m.rule_y, m.w - 8, m.rule_y, theme::color::hair, grip_a, 1.0);

    draw::brackets(cr, m.well.x, m.well.y, m.well.w, m.well.h, m.bracket, theme::color::hair,
                   theme::alpha::hair, 1.0);
    draw::range_ticks(cr, m.well.x, m.well.y, m.well.w, m.well.h, theme::hud::grid_x,
                      theme::hud::grid_y, theme::color::hair, theme::alpha::grid, 1.0);
    draw_constellation(cr, m);

    const char* pose =
        snap_.status.empty() ? pose_label(snap_.pose).data() : snap_.status.data();
    draw::label_mono(cr, theme::type::label, theme::color::paper, theme::alpha::text, m.pose.x,
                     m.pose.y, pose);
    const int filled = static_cast<int>(
        std::lround(std::clamp(snap_.command.confidence, 0.f, 1.f) * theme::hud::segs));
    draw::segments(cr, m.seg0.x, m.seg0.y, filled, 1.f);
    const auto conf = draw::conf_text(snap_.command.confidence);
    draw::label_mono(cr, theme::type::micro, theme::color::dim, theme::alpha::muted, m.conf.x,
                     m.conf.y, conf.c_str());

    if (!chip_.empty()) {
      draw::chip_brackets(cr, m.w * 0.5, m.chip.y, chip_.c_str(), 1.f);
    }

    cairo_set_operator(cr_, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr_, offscreen_, 0, 0);
    cairo_paint(cr_);
    cairo_surface_flush(surface_);
    XFlush(dpy_);
  }

  void draw_constellation(cairo_t* cr, const HudMetrics& m) {
    if (!snap_.hand) {
      const char* empty = "NO HAND";
      if (!snap_.camera_ok) {
        empty = snap_.status.empty() ? "NO CAM" : snap_.status.data();
      }
      draw::label_mono(cr, theme::type::label,
                       snap_.camera_ok ? theme::color::dim : theme::color::warn,
                       theme::alpha::empty, m.well.x + 6, m.well.y + m.well.h * 0.52, empty);
      if (!snap_.message.empty()) {
        const std::string clipped = snap_.message.size() > 34
                                        ? snap_.message.substr(0, 31) + "..."
                                        : snap_.message;
        draw::label_mono(cr, theme::type::micro, theme::color::dim, theme::alpha::empty,
                         m.well.x + 6, m.well.y + m.well.h * 0.72, clipped.c_str());
      }
      return;
    }
    const auto& lm = snap_.hand->landmarks;
    auto px = [&](int i) { return m.well.x + (1.0 - lm[static_cast<size_t>(i)].x) * m.well.w; };
    auto py = [&](int i) { return m.well.y + lm[static_cast<size_t>(i)].y * m.well.h; };
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (const auto& e : kHandBones) {
      const bool ray = index_ray(e[0], e[1]);
      draw::hairline(cr, px(e[0]), py(e[0]), px(e[1]), py(e[1]),
                     ray ? theme::color::accent : theme::color::hair,
                     ray ? theme::alpha::ray : theme::alpha::bone, 1.0);
    }
    for (int i = 0; i < kLandmarkCount; ++i) {
      const bool tip = i == kIndexTip;
      draw::dot(cr, px(i), py(i), tip ? 2.4 : 1.4, tip ? theme::color::accent : theme::color::paper,
                tip ? theme::alpha::index_tip : theme::alpha::tip);
    }
  }

  Display* dpy_ = nullptr;
  int screen_ = 0;
  Window root_ = 0;
  Window win_ = 0;
  Visual* visual_ = nullptr;
  int depth_ = 32;
  int x_ = 0;
  int y_ = 0;
  Cursor grab_cursor_ = 0;
  Cursor arrow_cursor_ = 0;
  cairo_surface_t* surface_ = nullptr;
  cairo_t* cr_ = nullptr;
  cairo_surface_t* offscreen_ = nullptr;
  cairo_t* off_cr_ = nullptr;
  TrackingSnapshot snap_{};
  std::string chip_;
  std::chrono::steady_clock::time_point chip_until_{};
  std::chrono::steady_clock::time_point last_tick_{};
  bool visible_ = false;
  bool dirty_ = true;
  bool grabbed_ = false;
  HudConfig hud_{};
  HudDrag drag_{};
  WorkArea work_{};
  std::function<void(HudConfig)> on_moved_;
};

}  // namespace

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<X11Overlay>(); }

}  // namespace airmouse

#endif
