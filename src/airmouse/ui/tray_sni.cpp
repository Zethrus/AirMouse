#include "airmouse/ui/tray.hpp"

#ifndef _WIN32

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <dbus/dbus.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "airmouse/ui/dbusmenu.hpp"
#include "airmouse/ui/icon.hpp"
#include "airmouse/ui/theme.hpp"

namespace airmouse {
namespace {

struct IconPix {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> argb;  // network-order ARGB bytes
};

IconPix load_icon_pixmap(int size) {
  IconPix out;
  const auto path = app_icon_path(size);
  if (path.empty()) return out;
  cairo_surface_t* src = cairo_image_surface_create_from_png(path.c_str());
  if (!src || cairo_surface_status(src) != CAIRO_STATUS_SUCCESS) {
    if (src) cairo_surface_destroy(src);
    return out;
  }
  const int sw = cairo_image_surface_get_width(src);
  const int sh = cairo_image_surface_get_height(src);
  cairo_surface_t* dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
  cairo_t* cr = cairo_create(dst);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);
  const double s = std::min(static_cast<double>(size) / std::max(1, sw),
                            static_cast<double>(size) / std::max(1, sh));
  cairo_translate(cr, (size - sw * s) * 0.5, (size - sh * s) * 0.5);
  cairo_scale(cr, s, s);
  cairo_set_source_surface(cr, src, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  cairo_surface_flush(dst);
  const auto* px = reinterpret_cast<const uint32_t*>(cairo_image_surface_get_data(dst));
  const int stride = cairo_image_surface_get_stride(dst) / 4;
  out.w = size;
  out.h = size;
  out.argb.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
  size_t o = 0;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const uint32_t p = px[y * stride + x];
      const uint8_t b = static_cast<uint8_t>(p);
      const uint8_t g = static_cast<uint8_t>(p >> 8);
      const uint8_t r = static_cast<uint8_t>(p >> 16);
      const uint8_t a = static_cast<uint8_t>(p >> 24);
      out.argb[o++] = a;
      out.argb[o++] = r;
      out.argb[o++] = g;
      out.argb[o++] = b;
    }
  }
  cairo_surface_destroy(dst);
  cairo_surface_destroy(src);
  return out;
}

void append_sv(DBusMessageIter* parent, const char* key, const char* sig,
               const auto& write_value) {
  DBusMessageIter ent;
  DBusMessageIter var;
  dbus_message_iter_open_container(parent, DBUS_TYPE_DICT_ENTRY, nullptr, &ent);
  dbus_message_iter_append_basic(&ent, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&ent, DBUS_TYPE_VARIANT, sig, &var);
  write_value(&var);
  dbus_message_iter_close_container(&ent, &var);
  dbus_message_iter_close_container(parent, &ent);
}

void append_icon_array(DBusMessageIter* parent, const std::vector<IconPix>& icons) {
  DBusMessageIter arr;
  dbus_message_iter_open_container(parent, DBUS_TYPE_ARRAY, "(iiay)", &arr);
  for (const auto& icon : icons) {
    if (icon.argb.empty()) continue;
    DBusMessageIter st;
    DBusMessageIter bytes;
    dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, nullptr, &st);
    dbus_int32_t w = icon.w;
    dbus_int32_t h = icon.h;
    dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &w);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &h);
    dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "y", &bytes);
    const uint8_t* data = icon.argb.data();
    dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &data,
                                         static_cast<int>(icon.argb.size()));
    dbus_message_iter_close_container(&st, &bytes);
    dbus_message_iter_close_container(&arr, &st);
  }
  dbus_message_iter_close_container(parent, &arr);
}

constexpr const char* kIntrospect = R"xml(<!DOCTYPE node PUBLIC
 "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg type="s" name="xml" direction="out"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get">
      <arg type="s" name="interface" direction="in"/>
      <arg type="s" name="property" direction="in"/>
      <arg type="v" name="value" direction="out"/>
    </method>
    <method name="GetAll">
      <arg type="s" name="interface" direction="in"/>
      <arg type="a{sv}" name="properties" direction="out"/>
    </method>
    <method name="Set">
      <arg type="s" name="interface" direction="in"/>
      <arg type="s" name="property" direction="in"/>
      <arg type="v" name="value" direction="in"/>
    </method>
  </interface>
  <interface name="org.kde.StatusNotifierItem">
    <method name="ContextMenu">
      <arg type="i" name="x" direction="in"/>
      <arg type="i" name="y" direction="in"/>
    </method>
    <method name="Activate">
      <arg type="i" name="x" direction="in"/>
      <arg type="i" name="y" direction="in"/>
    </method>
    <method name="SecondaryActivate">
      <arg type="i" name="x" direction="in"/>
      <arg type="i" name="y" direction="in"/>
    </method>
    <method name="Scroll">
      <arg type="i" name="delta" direction="in"/>
      <arg type="s" name="orientation" direction="in"/>
    </method>
    <property name="Category" type="s" access="read"/>
    <property name="Id" type="s" access="read"/>
    <property name="Title" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="WindowId" type="i" access="read"/>
    <property name="IconName" type="s" access="read"/>
    <property name="IconPixmap" type="a(iiay)" access="read"/>
    <property name="OverlayIconName" type="s" access="read"/>
    <property name="AttentionIconName" type="s" access="read"/>
    <property name="AttentionMovieName" type="s" access="read"/>
    <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
    <property name="ItemIsMenu" type="b" access="read"/>
    <property name="Menu" type="o" access="read"/>
  </interface>
</node>
)xml";

class SniTray final : public Tray {
 public:
  ~SniTray() override {
    hide_menu();
    menu_.detach();
    if (conn_ && registered_path_) {
      dbus_connection_unregister_object_path(conn_, "/StatusNotifierItem");
    }
    if (conn_) dbus_connection_unref(conn_);
    if (dpy_) XCloseDisplay(dpy_);
  }

  bool create() override {
    icons_.push_back(load_icon_pixmap(32));
    icons_.push_back(load_icon_pixmap(64));

    DBusError err;
    dbus_error_init(&err);
    conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn_ || dbus_error_is_set(&err)) {
      if (dbus_error_is_set(&err)) dbus_error_free(&err);
      conn_ = nullptr;
      return false;
    }
    dbus_connection_set_exit_on_disconnect(conn_, false);

    service_ = "org.airmouse.StatusNotifierItem";
    dbus_bus_request_name(conn_, service_.c_str(),
                          DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) dbus_error_free(&err);

    DBusObjectPathVTable vtable{};
    vtable.message_function = &SniTray::on_message;
    if (!dbus_connection_try_register_object_path(conn_, "/StatusNotifierItem", &vtable, this,
                                                  &err)) {
      if (dbus_error_is_set(&err)) dbus_error_free(&err);
      return false;
    }
    registered_path_ = true;
    menu_.attach(conn_, "/MenuBar");

    dbus_bus_add_match(
        conn_,
        "type='signal',sender='org.freedesktop.DBus',member='NameOwnerChanged',"
        "arg0='org.kde.StatusNotifierWatcher'",
        nullptr);
    register_with_watcher();
    return true;
  }

  void set_status(const std::string& text) override {
    tooltip_ = text;
    menu_.set_paused(text == "paused");
  }

  void set_handler(std::function<void(TrayAction)> handler) override {
    handler_ = std::move(handler);
    menu_.set_handler(handler_);
  }

  void poll() override {
    if (conn_) {
      dbus_connection_read_write(conn_, 0);
      while (dbus_connection_dispatch(conn_) == DBUS_DISPATCH_DATA_REMAINS) {
      }
    }
    poll_menu();
  }

 private:
  static DBusHandlerResult on_message(DBusConnection*, DBusMessage* msg, void* data) {
    return static_cast<SniTray*>(data)->handle(msg);
  }

  DBusHandlerResult handle(DBusMessage* msg) {
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
      DBusMessage* reply = dbus_message_new_method_return(msg);
      const char* xml = kIntrospect;
      dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
      dbus_connection_send(conn_, reply, nullptr);
      dbus_message_unref(reply);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
      reply_get(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll")) {
      reply_get_all(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Set")) {
      DBusMessage* reply = dbus_message_new_method_return(msg);
      dbus_connection_send(conn_, reply, nullptr);
      dbus_message_unref(reply);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Activate")) {
      if (handler_) handler_(TrayAction::TogglePause);
      reply_void(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "SecondaryActivate")) {
      if (handler_) handler_(TrayAction::ToggleHud);
      reply_void(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "ContextMenu")) {
      dbus_int32_t x = 0;
      dbus_int32_t y = 0;
      dbus_message_get_args(msg, nullptr, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y,
                            DBUS_TYPE_INVALID);
      show_menu(x, y);
      reply_void(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Scroll")) {
      reply_void(msg);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged")) {
      register_with_watcher();
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  void reply_void(DBusMessage* msg) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (reply) {
      dbus_connection_send(conn_, reply, nullptr);
      dbus_message_unref(reply);
    }
  }

  void write_prop(DBusMessageIter* var, const char* name) {
    if (std::strcmp(name, "Category") == 0) {
      const char* v = "ApplicationStatus";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "Id") == 0) {
      const char* v = "airmouse";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "Title") == 0) {
      const char* v = "AirMouse";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "Status") == 0) {
      const char* v = "Active";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "WindowId") == 0) {
      dbus_int32_t v = 0;
      dbus_message_iter_append_basic(var, DBUS_TYPE_INT32, &v);
    } else if (std::strcmp(name, "IconName") == 0) {
      const char* v = "airmouse";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "IconPixmap") == 0) {
      append_icon_array(var, icons_);
    } else if (std::strcmp(name, "OverlayIconName") == 0 ||
               std::strcmp(name, "AttentionIconName") == 0 ||
               std::strcmp(name, "AttentionMovieName") == 0) {
      const char* v = "";
      dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
    } else if (std::strcmp(name, "ItemIsMenu") == 0) {
      dbus_bool_t v = FALSE;
      dbus_message_iter_append_basic(var, DBUS_TYPE_BOOLEAN, &v);
    } else if (std::strcmp(name, "Menu") == 0) {
      const char* v = "/MenuBar";
      dbus_message_iter_append_basic(var, DBUS_TYPE_OBJECT_PATH, &v);
    } else if (std::strcmp(name, "ToolTip") == 0) {
      DBusMessageIter st;
      dbus_message_iter_open_container(var, DBUS_TYPE_STRUCT, nullptr, &st);
      const char* icon = "airmouse";
      dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &icon);
      append_icon_array(&st, icons_);
      const char* title = "AirMouse";
      const char* tip = tooltip_.c_str();
      dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &title);
      dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &tip);
      dbus_message_iter_close_container(var, &st);
    }
  }

  void reply_get(DBusMessage* msg) {
    const char* iface = nullptr;
    const char* name = nullptr;
    dbus_message_get_args(msg, nullptr, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &name,
                          DBUS_TYPE_INVALID);
    DBusMessage* reply = dbus_message_new_method_return(msg);
    DBusMessageIter it;
    DBusMessageIter var;
    dbus_message_iter_init_append(reply, &it);
    const char* sig = "s";
    if (name && std::strcmp(name, "IconPixmap") == 0) sig = "a(iiay)";
    else if (name && std::strcmp(name, "WindowId") == 0) sig = "i";
    else if (name && std::strcmp(name, "ItemIsMenu") == 0) sig = "b";
    else if (name && std::strcmp(name, "Menu") == 0) sig = "o";
    else if (name && std::strcmp(name, "ToolTip") == 0) sig = "(sa(iiay)ss)";
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, sig, &var);
    if (name) write_prop(&var, name);
    dbus_message_iter_close_container(&it, &var);
    dbus_connection_send(conn_, reply, nullptr);
    dbus_message_unref(reply);
  }

  void reply_get_all(DBusMessage* msg) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    DBusMessageIter it;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &it);
    dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &dict);
    const char* props[] = {"Category",          "Id",
                           "Title",             "Status",
                           "WindowId",          "IconName",
                           "IconPixmap",        "OverlayIconName",
                           "AttentionIconName", "AttentionMovieName",
                           "ToolTip",           "ItemIsMenu",
                           "Menu"};
    const char* sigs[] = {"s", "s", "s", "s", "i", "s", "a(iiay)", "s", "s", "s",
                          "(sa(iiay)ss)", "b", "o"};
    for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); ++i) {
      append_sv(&dict, props[i], sigs[i], [&](DBusMessageIter* var) { write_prop(var, props[i]); });
    }
    dbus_message_iter_close_container(&it, &dict);
    dbus_connection_send(conn_, reply, nullptr);
    dbus_message_unref(reply);
  }

  void register_with_watcher() {
    if (!conn_) return;
    DBusMessage* msg = dbus_message_new_method_call(
        "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem");
    if (!msg) return;
    const char* svc = service_.c_str();
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &svc, DBUS_TYPE_INVALID);
    dbus_connection_send(conn_, msg, nullptr);
    dbus_message_unref(msg);
    dbus_connection_flush(conn_);
  }

  struct MenuItem {
    const char* label;
    TrayAction action;
    bool sep;
  };

  static constexpr MenuItem kItems[] = {
      {"Pause / Resume", TrayAction::TogglePause, false},
      {"Toggle HUD", TrayAction::ToggleHud, false},
      {"Settings", TrayAction::OpenSettings, false},
      {"Calibrate", TrayAction::Calibrate, false},
      {"Quit", TrayAction::Quit, true},
  };

  void ensure_display() {
    if (!dpy_) dpy_ = XOpenDisplay(nullptr);
  }

  void show_menu(int x, int y) {
    ensure_display();
    if (!dpy_) return;
    hide_menu();
    if (x == 0 && y == 0) {
      Window root = DefaultRootWindow(dpy_);
      Window child = 0;
      int wx = 0;
      int wy = 0;
      unsigned mask = 0;
      XQueryPointer(dpy_, root, &root, &child, &x, &y, &wx, &wy, &mask);
    }
    const int item_h = 30;
    const int pad = 8;
    menu_w_ = 188;
    menu_h_ = pad * 2 + static_cast<int>(std::size(kItems)) * item_h;
    menu_x_ = x;
    menu_y_ = y - menu_h_;
    if (menu_y_ < 8) menu_y_ = y + 8;

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy_, DefaultScreen(dpy_), 32, TrueColor, &vinfo)) {
      XMatchVisualInfo(dpy_, DefaultScreen(dpy_), 24, TrueColor, &vinfo);
    }
    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy_, DefaultRootWindow(dpy_), vinfo.visual, AllocNone);
    swa.border_pixel = 0;
    swa.background_pixel = 0;
    swa.override_redirect = True;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     LeaveWindowMask | KeyPressMask;
    menu_win_ = XCreateWindow(dpy_, DefaultRootWindow(dpy_), menu_x_, menu_y_, menu_w_, menu_h_, 0,
                              vinfo.depth, InputOutput, vinfo.visual,
                              CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect |
                                  CWEventMask,
                              &swa);
    Atom type = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE", False);
    Atom popup = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    XChangeProperty(dpy_, menu_win_, type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&popup), 1);
    XMapRaised(dpy_, menu_win_);
    if (XGrabPointer(dpy_, menu_win_, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
      // Still show the menu; click-off dismiss may be less reliable.
    }
    draw_menu(-1);
  }

  void hide_menu() {
    if (!dpy_ || !menu_win_) return;
    XUngrabPointer(dpy_, CurrentTime);
    XDestroyWindow(dpy_, menu_win_);
    menu_win_ = 0;
    XFlush(dpy_);
  }

  int hit_item(int y) const {
    const int pad = 8;
    const int item_h = 30;
    const int idx = (y - pad) / item_h;
    if (idx < 0 || idx >= static_cast<int>(std::size(kItems))) return -1;
    return idx;
  }

  void draw_menu(int hover) {
    if (!dpy_ || !menu_win_) return;
    XWindowAttributes wa{};
    XGetWindowAttributes(dpy_, menu_win_, &wa);
    Visual* visual = wa.visual ? wa.visual : DefaultVisual(dpy_, DefaultScreen(dpy_));
    cairo_surface_t* surface =
        cairo_xlib_surface_create(dpy_, menu_win_, visual, menu_w_, menu_h_);
    cairo_xlib_surface_set_size(surface, menu_w_, menu_h_);
    cairo_surface_t* img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, menu_w_, menu_h_);
    cairo_t* cr = cairo_create(img);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    auto rr = [&](double x, double y, double w, double h, double r) {
      cairo_new_sub_path(cr);
      cairo_arc(cr, x + w - r, y + r, r, -1.5708, 0);
      cairo_arc(cr, x + w - r, y + h - r, r, 0, 1.5708);
      cairo_arc(cr, x + r, y + h - r, r, 1.5708, 3.1416);
      cairo_arc(cr, x + r, y + r, r, 3.1416, 4.7124);
      cairo_close_path(cr);
    };
    rr(0.5, 0.5, menu_w_ - 1.0, menu_h_ - 1.0, 12);
    cairo_set_source_rgba(cr, 0.043, 0.047, 0.059, 0.94);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.91, 0.90, 0.88, 0.14);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    const int pad = 8;
    const int item_h = 30;
    cairo_select_font_face(cr, "IBM Plex Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    for (int i = 0; i < static_cast<int>(std::size(kItems)); ++i) {
      const double iy = pad + i * item_h;
      if (kItems[i].sep && i > 0) {
        cairo_set_source_rgba(cr, 0.91, 0.90, 0.88, 0.10);
        cairo_move_to(cr, 14, iy);
        cairo_line_to(cr, menu_w_ - 14, iy);
        cairo_stroke(cr);
      }
      if (i == hover) {
        rr(8, iy + 2, menu_w_ - 16, item_h - 4, 8);
        cairo_set_source_rgba(cr, 0.769, 0.647, 0.455, 0.22);
        cairo_fill(cr);
      }
      cairo_set_source_rgba(cr, 0.91, 0.90, 0.88, 0.92);
      cairo_move_to(cr, 18, iy + 20);
      cairo_show_text(cr, kItems[i].label);
    }
    cairo_destroy(cr);
    cairo_t* win = cairo_create(surface);
    cairo_set_operator(win, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(win, img, 0, 0);
    cairo_paint(win);
    cairo_destroy(win);
    cairo_surface_destroy(img);
    cairo_surface_destroy(surface);
    XFlush(dpy_);
  }

  void poll_menu() {
    if (!dpy_ || !menu_win_) return;
    while (XPending(dpy_)) {
      XEvent ev;
      XNextEvent(dpy_, &ev);
      if (ev.type == Expose) {
        draw_menu(hover_);
      } else if (ev.type == MotionNotify) {
        const int h = hit_item(ev.xmotion.y);
        if (h != hover_) {
          hover_ = h;
          draw_menu(hover_);
        }
      } else if (ev.type == LeaveNotify) {
        hover_ = -1;
        draw_menu(hover_);
      } else if (ev.type == ButtonRelease || ev.type == ButtonPress) {
        if (ev.xbutton.window != menu_win_ || ev.xbutton.x < 0 || ev.xbutton.y < 0 ||
            ev.xbutton.x >= menu_w_ || ev.xbutton.y >= menu_h_) {
          hide_menu();
          break;
        }
        if (ev.type == ButtonRelease) {
          const int idx = hit_item(ev.xbutton.y);
          hide_menu();
          if (idx >= 0 && handler_) handler_(kItems[idx].action);
          break;
        }
      } else if (ev.type == KeyPress) {
        hide_menu();
        break;
      }
    }
  }

  DBusConnection* conn_ = nullptr;
  DbusMenu menu_;
  Display* dpy_ = nullptr;
  Window menu_win_ = 0;
  int menu_x_ = 0;
  int menu_y_ = 0;
  int menu_w_ = 188;
  int menu_h_ = 160;
  int hover_ = -1;
  bool registered_path_ = false;
  std::string service_;
  std::string tooltip_ = "AirMouse";
  std::function<void(TrayAction)> handler_;
  std::vector<IconPix> icons_;
};

}  // namespace

std::unique_ptr<Tray> create_tray() { return std::make_unique<SniTray>(); }

}  // namespace airmouse

#endif
