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
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

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
    x_ = (sw - kW) / 2;
    y_ = (sh - kH) / 3;
    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy_, DefaultRootWindow(dpy_), vinfo.visual, AllocNone);
    swa.border_pixel = 0;
    swa.background_pixel = 0;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     KeyPressMask | StructureNotifyMask;
    win_ = XCreateWindow(dpy_, DefaultRootWindow(dpy_), x_, y_, kW, kH, 0, vinfo.depth,
                         InputOutput, vinfo.visual,
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
  static constexpr int kW = 380;
  static constexpr int kH = 392;

  struct Hit {
    const char* id;
    int x, y, w, h;
  };

  std::vector<Hit> layout() const {
    return {
        {"cam", 24, 86, kW - 48, 28},
        {"hud", 24, 136, 160, 24},
        {"chips", 24, 168, 160, 24},
        {"mirror", 24, 200, 160, 24},
        {"box", 24, 252, kW - 48, 18},
        {"open", 24, kH - 64, 140, 32},
        {"done", kW - 116, kH - 64, 92, 32},
    };
  }

  void apply() {
    try {
      save_config(default_config_path(), *cfg_);
    } catch (...) {
    }
    if (cb_) cb_(*cfg_);
  }

  void on_click(int x, int y) {
    for (const auto& h : layout()) {
      if (x < h.x || y < h.y || x > h.x + h.w || y > h.y + h.h) continue;
      if (std::strcmp(h.id, "hud") == 0) cfg_->hud.enabled = !cfg_->hud.enabled;
      else if (std::strcmp(h.id, "chips") == 0) cfg_->hud.chips = !cfg_->hud.chips;
      else if (std::strcmp(h.id, "mirror") == 0) cfg_->mirror = !cfg_->mirror;
      else if (std::strcmp(h.id, "cam") == 0) {
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
    cairo_surface_t* xs = cairo_xlib_surface_create(dpy_, win_, visual, kW, kH);
    cairo_surface_t* img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kW, kH);
    cairo_t* cr = cairo_create(img);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    set_hex(cr, theme::kVoid, 1.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    set_hex(cr, theme::kPaper, 0.92);
    cairo_select_font_face(cr, "IBM Plex Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, 24, 40);
    cairo_show_text(cr, "AirMouse");
    set_hex(cr, theme::kDim, 0.85);
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, 24, 60);
    cairo_show_text(cr, "Pointer, camera, and overlay");

    auto chip = [&](const Hit& hit, const std::string& text, bool accent) {
      rounded_rect(cr, hit.x, hit.y, hit.w, hit.h, 8);
      set_hex(cr, accent ? theme::kAccent : theme::kHair, accent ? 0.22 : 0.08);
      cairo_fill(cr);
      set_hex(cr, accent ? theme::kAccent : theme::kPaper, 0.92);
      cairo_set_font_size(cr, 12);
      cairo_move_to(cr, hit.x + 12, hit.y + 19);
      cairo_show_text(cr, text.c_str());
    };

    if (cameras_.empty()) {
      set_hex(cr, theme::kDim, 0.85);
      cairo_set_font_size(cr, 10);
      cairo_move_to(cr, 24, 122);
      cairo_show_text(cr, "Press the laptop camera key, then click the camera row");
    }

    for (const auto& h : layout()) {
      if (std::strcmp(h.id, "cam") == 0) chip(h, camera_label(), false);
      else if (std::strcmp(h.id, "hud") == 0) chip(h, cfg_->hud.enabled ? "HUD  on" : "HUD  off", cfg_->hud.enabled);
      else if (std::strcmp(h.id, "chips") == 0)
        chip(h, cfg_->hud.chips ? "Chips  on" : "Chips  off", cfg_->hud.chips);
      else if (std::strcmp(h.id, "mirror") == 0)
        chip(h, cfg_->mirror ? "Mirror  on" : "Mirror  off", cfg_->mirror);
      else if (std::strcmp(h.id, "box") == 0) {
        set_hex(cr, theme::kDim, 0.85);
        cairo_set_font_size(cr, 11);
        cairo_move_to(cr, h.x, h.y - 8);
        cairo_show_text(cr, "Control box");
        rounded_rect(cr, h.x, h.y, h.w, h.h, 9);
        set_hex(cr, theme::kHair, 0.10);
        cairo_fill(cr);
        const float t = (cfg_->control_box - 0.35f) / 0.55f;
        rounded_rect(cr, h.x, h.y, h.w * std::clamp(t, 0.f, 1.f), h.h, 9);
        set_hex(cr, theme::kAccent, 0.85);
        cairo_fill(cr);
      } else if (std::strcmp(h.id, "open") == 0) chip(h, "Open config", false);
      else if (std::strcmp(h.id, "done") == 0) chip(h, "Done", true);
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
