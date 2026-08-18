#include "airmouse/ui/tray.hpp"

#ifndef _WIN32

#include <dbus/dbus.h>

#include <cstring>

namespace airmouse {
namespace {

// Minimal StatusNotifierItem. Left-click pauses, right-click opens settings.
class SniTray final : public Tray {
 public:
  ~SniTray() override {
    if (conn_) {
      dbus_connection_unref(conn_);
    }
  }

  bool create() override {
    DBusError err;
    dbus_error_init(&err);
    conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn_ || dbus_error_is_set(&err)) {
      if (dbus_error_is_set(&err)) dbus_error_free(&err);
      conn_ = nullptr;
      return false;
    }
    dbus_connection_set_exit_on_disconnect(conn_, false);

    const std::string name = "org.airmouse.StatusNotifierItem";
    const int flags = DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE;
    dbus_bus_request_name(conn_, name.c_str(), flags, &err);
    if (dbus_error_is_set(&err)) dbus_error_free(&err);

    dbus_bus_add_match(
        conn_,
        "type='method_call',interface='org.kde.StatusNotifierItem',path='/StatusNotifierItem'",
        &err);
    if (dbus_error_is_set(&err)) dbus_error_free(&err);

    DBusMessage* msg = dbus_message_new_method_call(
        "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem");
    if (msg) {
      const char* svc = name.c_str();
      dbus_message_append_args(msg, DBUS_TYPE_STRING, &svc, DBUS_TYPE_INVALID);
      dbus_connection_send(conn_, msg, nullptr);
      dbus_message_unref(msg);
    }
    dbus_connection_flush(conn_);
    return true;
  }

  void set_status(const std::string& text) override { tooltip_ = text; }

  void set_handler(std::function<void(TrayAction)> handler) override {
    handler_ = std::move(handler);
  }

  void poll() override {
    if (!conn_) return;
    dbus_connection_read_write(conn_, 0);
    DBusMessage* msg = nullptr;
    while ((msg = dbus_connection_pop_message(conn_)) != nullptr) {
      if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Activate")) {
        if (handler_) handler_(TrayAction::TogglePause);
        reply_void(msg);
      } else if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem",
                                             "ContextMenu")) {
        if (handler_) handler_(TrayAction::OpenSettings);
        reply_void(msg);
      } else if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem",
                                             "SecondaryActivate")) {
        if (handler_) handler_(TrayAction::ToggleHud);
        reply_void(msg);
      } else if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
        reply_void(msg);
      }
      dbus_message_unref(msg);
    }
  }

 private:
  void reply_void(DBusMessage* msg) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (reply) {
      dbus_connection_send(conn_, reply, nullptr);
      dbus_message_unref(reply);
    }
  }

  DBusConnection* conn_ = nullptr;
  std::function<void(TrayAction)> handler_;
  std::string tooltip_ = "AirMouse";
};

}  // namespace

std::unique_ptr<Tray> create_tray() { return std::make_unique<SniTray>(); }

}  // namespace airmouse

#endif
