#pragma once

#ifndef _WIN32

#include <dbus/dbus.h>

#include <functional>

#include "airmouse/ui/tray.hpp"

namespace airmouse {

// com.canonical.dbusmenu exporter. GNOME AppIndicator only shows a tray
// menu when StatusNotifierItem.Menu points at one of these.
class DbusMenu {
 public:
  DbusMenu() = default;
  ~DbusMenu();

  DbusMenu(const DbusMenu&) = delete;
  DbusMenu& operator=(const DbusMenu&) = delete;

  bool attach(DBusConnection* conn, const char* path = "/MenuBar");
  void detach();
  void set_handler(std::function<void(TrayAction)> handler);
  void set_paused(bool paused);

 private:
  static DBusHandlerResult on_message(DBusConnection* conn, DBusMessage* msg, void* data);
  DBusHandlerResult handle(DBusMessage* msg);
  void reply_void(DBusMessage* msg);
  void reply_get_layout(DBusMessage* msg);
  void reply_get_group_properties(DBusMessage* msg);
  void reply_get_property(DBusMessage* msg);
  void reply_about_to_show(DBusMessage* msg);
  void reply_about_to_show_group(DBusMessage* msg);
  void reply_event(DBusMessage* msg);
  void reply_event_group(DBusMessage* msg);
  void reply_props_get(DBusMessage* msg);
  void reply_props_get_all(DBusMessage* msg);
  void reply_introspect(DBusMessage* msg);
  void handle_clicked(int id);

  DBusConnection* conn_ = nullptr;
  const char* path_ = nullptr;
  bool registered_ = false;
  bool paused_ = false;
  dbus_uint32_t revision_ = 1;
  std::function<void(TrayAction)> handler_;
};

}  // namespace airmouse

#endif
