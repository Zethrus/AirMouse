#include "airmouse/ui/settings.hpp"

#ifndef _WIN32

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "airmouse/camera/capture.hpp"
#include "airmouse/ui/cairo_draw.hpp"
#include "airmouse/ui/settings_layout.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

class X11Settings final : public SettingsWindow {
 public:
  explicit X11Settings(Config* cfg) : cfg_(cfg) {}
  ~X11Settings() override { hide(); }

  void show() override {
    if (visible_) {
      if (dpy_ && win_) XMapRaised(dpy_, win_);
      return;
    }
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_ || !cfg_) return;
    refresh_cameras();

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy_, DefaultScreen(dpy_), 32, TrueColor, &vinfo)) {
      if (!XMatchVisualInfo(dpy_, DefaultScreen(dpy_), 24, TrueColor, &vinfo)) {
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
        return;
      }
    }
    const int sw = DisplayWidth(dpy_, DefaultScreen(dpy_));
    const int sh = DisplayHeight(dpy_, DefaultScreen(dpy_));
    x_ = (sw - theme::settings::w) / 2;
    y_ = (sh - theme::settings::h) / 3;
    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy_, DefaultRootWindow(dpy_), vinfo.visual, AllocNone);
    swa.border_pixel = 0;
    swa.background_pixel = 0;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     KeyPressMask | StructureNotifyMask;
    win_ = XCreateWindow(dpy_, DefaultRootWindow(dpy_), x_, y_, theme::settings::w,
                         theme::settings::h, 0, vinfo.depth, InputOutput, vinfo.visual,
                         CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
    XStoreName(dpy_, win_, "AirMouse settings");
    wm_delete_ = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wm_delete_, 1);
    XClassHint hint{};
    hint.res_name = const_cast<char*>("airmouse");
    hint.res_class = const_cast<char*>("AirMouse");
    XSetClassHint(dpy_, win_, &hint);
    XMapRaised(dpy_, win_);
    visible_ = true;
    draw();
  }

  void hide() override {
    if (dpy_ && win_) XDestroyWindow(dpy_, win_);
    if (dpy_) XCloseDisplay(dpy_);
    dpy_ = nullptr;
    win_ = 0;
    visible_ = false;
  }

  bool visible() const override { return visible_; }

  void poll() override {
    if (!dpy_ || !win_) return;
    while (XPending(dpy_)) {
      XEvent ev;
      XNextEvent(dpy_, &ev);
      if (ev.type == ClientMessage && static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete_) {
        hide();
        return;
      }
      if (ev.type == Expose) draw();
      if (ev.type == ButtonRelease) on_click(ev.xbutton.x, ev.xbutton.y);
      if (ev.type == KeyPress) {
        KeySym ks = XLookupKeysym(&ev.xkey, 0);
        if (ks == XK_Escape) {
          hide();
          return;
        }
      }
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
    for (const auto& h : settings_hits()) {
      if (!hit_contains(h, x, y)) continue;
      if (std::strcmp(h.id, "hud") == 0) cfg_->hud.enabled = !cfg_->hud.enabled;
      else if (std::strcmp(h.id, "chips") == 0) cfg_->hud.chips = !cfg_->hud.chips;
      else if (std::strcmp(h.id, "mirror") == 0) cfg_->mirror = !cfg_->mirror;
      else if (std::strcmp(h.id, "reset") == 0) {
        cfg_->hud.placed = false;
        cfg_->hud.x = 0;
        cfg_->hud.y = 0;
      } else if (std::strcmp(h.id, "cam") == 0) {
        refresh_cameras();
        const int n = static_cast<int>(cameras_.size()) + 1;
        cam_sel_ = (cam_sel_ + 1) % std::max(1, n);
        cfg_->camera_index = cam_sel_ == 0 ? -1 : cameras_[static_cast<size_t>(cam_sel_ - 1)].index;
      } else if (std::strcmp(h.id, "box") == 0) {
        const float t = std::clamp((x - h.x) / static_cast<float>(h.w), 0.f, 1.f);
        cfg_->control_box = 0.35f + t * 0.55f;
      } else if (std::strcmp(h.id, "open") == 0) {
        const auto path = default_config_path();
        try {
          save_config(path, *cfg_);
        } catch (...) {
        }
        const std::string cmd = "xdg-open \"" + path.parent_path().string() + "\" >/dev/null 2>&1 &";
        const int rc = std::system(cmd.c_str());
        (void)rc;
      } else if (std::strcmp(h.id, "done") == 0) {
        apply();
        hide();
        return;
      }
      apply();
      draw();
      return;
    }
  }

  void refresh_cameras() {
    cameras_ = list_camera_devices();
    cam_sel_ = 0;
    if (!cfg_) return;
    for (size_t i = 0; i < cameras_.size(); ++i) {
      if (cameras_[i].index == cfg_->camera_index) cam_sel_ = static_cast<int>(i) + 1;
    }
  }

  std::string camera_label() const {
    if (cameras_.empty()) return "Camera  No camera";
    if (cam_sel_ == 0) {
      if (cameras_.size() == 1) return "Camera  Auto — " + cameras_.front().name;
      return "Camera  Auto";
    }
    return "Camera  " + cameras_[static_cast<size_t>(cam_sel_ - 1)].name;
  }

  void draw() {
    if (!dpy_ || !win_) return;
    XWindowAttributes wa{};
    XGetWindowAttributes(dpy_, win_, &wa);
    Visual* visual = wa.visual ? wa.visual : DefaultVisual(dpy_, DefaultScreen(dpy_));
    cairo_surface_t* xs =
        cairo_xlib_surface_create(dpy_, win_, visual, theme::settings::w, theme::settings::h);
    cairo_surface_t* img =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, theme::settings::w, theme::settings::h);
    cairo_t* cr = cairo_create(img);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    draw::set_hex(cr, theme::color::panel, 1.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    const int pad = theme::space::xl;
    draw::label_sans(cr, theme::type::title, theme::color::paper, theme::alpha::text, pad, 40,
                     "AirMouse");
    draw::label_mono(cr, theme::type::label, theme::color::dim, theme::alpha::muted, pad, 60,
                     "POINTER  CAMERA  OVERLAY");

    auto chip = [&](const SettingsHit& hit, const std::string& text, bool on) {
      draw::fill_round_rect(cr, hit.x, hit.y, hit.w, hit.h, theme::radius::chip,
                            on ? theme::color::accent : theme::color::hair,
                            on ? theme::alpha::chip_on : theme::alpha::chip_off);
      draw::label_mono(cr, theme::type::body, on ? theme::color::accent : theme::color::paper,
                       theme::alpha::text, hit.x + theme::space::md, hit.y + 17, text.c_str());
    };

    if (cameras_.empty()) {
      draw::label_mono(cr, theme::type::micro, theme::color::dim, theme::alpha::muted, pad, 122,
                       "Press the laptop camera key, then click the camera row");
    }

    for (const auto& h : settings_hits()) {
      if (std::strcmp(h.id, "cam") == 0) chip(h, camera_label(), false);
      else if (std::strcmp(h.id, "hud") == 0)
        chip(h, cfg_->hud.enabled ? "HUD  on" : "HUD  off", cfg_->hud.enabled);
      else if (std::strcmp(h.id, "chips") == 0)
        chip(h, cfg_->hud.chips ? "Chips  on" : "Chips  off", cfg_->hud.chips);
      else if (std::strcmp(h.id, "mirror") == 0)
        chip(h, cfg_->mirror ? "Mirror  on" : "Mirror  off", cfg_->mirror);
      else if (std::strcmp(h.id, "reset") == 0)
        chip(h, "Reset HUD position", false);
      else if (std::strcmp(h.id, "box") == 0) {
        draw::label_mono(cr, theme::type::label, theme::color::dim, theme::alpha::muted, h.x,
                         h.y - 8, "Control box");
        draw::fill_round_rect(cr, h.x, h.y, h.w, h.h, theme::radius::chip, theme::color::hair,
                              theme::alpha::hair_dim);
        const float t = (cfg_->control_box - 0.35f) / 0.55f;
        draw::fill_round_rect(cr, h.x, h.y, h.w * std::clamp(t, 0.f, 1.f), h.h, theme::radius::chip,
                              theme::color::accent, theme::alpha::slider);
      } else if (std::strcmp(h.id, "open") == 0)
        chip(h, "Open config", false);
      else if (std::strcmp(h.id, "done") == 0)
        chip(h, "Done", true);
    }

    cairo_destroy(cr);
    cairo_t* win = cairo_create(xs);
    cairo_set_operator(win, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(win, img, 0, 0);
    cairo_paint(win);
    cairo_destroy(win);
    cairo_surface_destroy(img);
    cairo_surface_destroy(xs);
    XFlush(dpy_);
  }

  Config* cfg_ = nullptr;
  std::function<void(const Config&)> cb_;
  Display* dpy_ = nullptr;
  Window win_ = 0;
  Atom wm_delete_ = 0;
  bool visible_ = false;
  int x_ = 0;
  int y_ = 0;
  std::vector<CameraDevice> cameras_;
  int cam_sel_ = 0;
};

}  // namespace

std::unique_ptr<SettingsWindow> create_x11_settings(Config* cfg) {
  return std::make_unique<X11Settings>(cfg);
}

}  // namespace airmouse

#endif
