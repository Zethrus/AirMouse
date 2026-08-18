#include "cammouse/pointer/screens.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

namespace cammouse {

#ifdef _WIN32
ScreenGeometry query_screen_geometry() {
  ScreenGeometry g;
  g.origin_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  g.origin_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  g.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  g.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (g.width <= 0) g.width = 1920;
  if (g.height <= 0) g.height = 1080;
  return g;
}
#else
ScreenGeometry query_screen_geometry() {
  ScreenGeometry g;
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    return g;
  }
  const int screen = DefaultScreen(dpy);
  g.origin_x = 0;
  g.origin_y = 0;
  g.width = DisplayWidth(dpy, screen);
  g.height = DisplayHeight(dpy, screen);
  XCloseDisplay(dpy);
  return g;
}
#endif

}  // namespace cammouse
