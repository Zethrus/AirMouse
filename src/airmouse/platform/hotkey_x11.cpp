#include "airmouse/platform/hotkey.hpp"

#ifndef _WIN32

#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace airmouse {
namespace {

class X11Hotkey final : public Hotkey {
 public:
  ~X11Hotkey() override {
    if (dpy_ && grabbed_) {
      const unsigned mods = ControlMask | Mod1Mask | ShiftMask;
      XUngrabKey(dpy_, keycode_, mods, root_);
      XUngrabKey(dpy_, keycode_, mods | LockMask, root_);
      XUngrabKey(dpy_, keycode_, mods | Mod2Mask, root_);
      XUngrabKey(dpy_, keycode_, mods | LockMask | Mod2Mask, root_);
    }
    if (dpy_) XCloseDisplay(dpy_);
  }

  bool grab() override {
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) return false;
    root_ = DefaultRootWindow(dpy_);
    keycode_ = XKeysymToKeycode(dpy_, XK_C);
    const unsigned mods = ControlMask | Mod1Mask | ShiftMask;
    const unsigned extras[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (unsigned extra : extras) {
      XGrabKey(dpy_, keycode_, mods | extra, root_, True, GrabModeAsync, GrabModeAsync);
    }
    XSelectInput(dpy_, root_, KeyPressMask);
    grabbed_ = true;
    return true;
  }

  void set_handler(std::function<void()> on_toggle) override {
    handler_ = std::move(on_toggle);
  }

  void poll() override {
    if (!dpy_) return;
    while (XPending(dpy_)) {
      XEvent ev;
      XNextEvent(dpy_, &ev);
      if (ev.type == KeyPress && handler_) handler_();
    }
  }

 private:
  Display* dpy_ = nullptr;
  Window root_ = 0;
  int keycode_ = 0;
  bool grabbed_ = false;
  std::function<void()> handler_;
};

}  // namespace

std::unique_ptr<Hotkey> create_hotkey() { return std::make_unique<X11Hotkey>(); }

}  // namespace airmouse

#endif
