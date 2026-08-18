#include "airmouse/platform/dbus_toggle.hpp"

#ifndef _WIN32

#include <dbus/dbus.h>

namespace airmouse {
namespace {

class DbusToggleImpl final : public DbusToggle {
 public:
  ~DbusToggleImpl() override {
    if (conn_) dbus_connection_unref(conn_);
  }

  bool advertise() override {
    DBusError err;
    dbus_error_init(&err);
    conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn_ || dbus_error_is_set(&err)) {
      if (dbus_error_is_set(&err)) dbus_error_free(&err);
      conn_ = nullptr;
      return false;
    }
    dbus_bus_request_name(conn_, "org.airmouse.App", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) dbus_error_free(&err);
    dbus_bus_add_match(conn_,
                       "type='method_call',interface='org.airmouse.App',path='/org/airmouse/App'",
                       nullptr);
    return true;
  }

  void poll() override {
    if (!conn_) return;
    dbus_connection_read_write(conn_, 0);
    DBusMessage* msg = nullptr;
    while ((msg = dbus_connection_pop_message(conn_)) != nullptr) {
      if (dbus_message_is_method_call(msg, "org.airmouse.App", "TogglePause")) {
        if (handler_) handler_();
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
          dbus_connection_send(conn_, reply, nullptr);
          dbus_message_unref(reply);
        }
      }
      dbus_message_unref(msg);
    }
  }

  void set_handler(std::function<void()> on_toggle) override { handler_ = std::move(on_toggle); }

 private:
  DBusConnection* conn_ = nullptr;
  std::function<void()> handler_;
};

}  // namespace

std::unique_ptr<DbusToggle> create_dbus_toggle() {
  return std::make_unique<DbusToggleImpl>();
}

}  // namespace airmouse

#else

namespace airmouse {

class NullDbus final : public DbusToggle {
 public:
  bool advertise() override { return false; }
  void poll() override {}
  void set_handler(std::function<void()>) override {}
};

std::unique_ptr<DbusToggle> create_dbus_toggle() { return std::make_unique<NullDbus>(); }

}  // namespace airmouse

#endif
