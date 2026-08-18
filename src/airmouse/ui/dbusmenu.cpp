#include "airmouse/ui/dbusmenu.hpp"

#ifndef _WIN32

#include <cstring>
#include <string>
#include <vector>

namespace airmouse {
namespace {

constexpr const char* kIface = "com.canonical.dbusmenu";

struct Item {
  int id;
  const char* type;   // "standard" or "separator"
  const char* label;  // may be null for separator
  TrayAction action;
  bool has_action;
};

constexpr Item kStaticItems[] = {
    {1, "standard", "Pause / Resume", TrayAction::TogglePause, true},
    {2, "standard", "Toggle HUD", TrayAction::ToggleHud, true},
    {3, "standard", "Settings", TrayAction::OpenSettings, true},
    {4, "standard", "Calibrate", TrayAction::Calibrate, true},
    {5, "separator", nullptr, TrayAction::Quit, false},
    {6, "standard", "Quit", TrayAction::Quit, true},
};

const Item* find_item(int id) {
  for (const auto& item : kStaticItems) {
    if (item.id == id) return &item;
  }
  return nullptr;
}

bool name_wanted(const char* name, char** filter, int nfilter) {
  if (nfilter <= 0) return true;
  for (int i = 0; i < nfilter; ++i) {
    if (filter[i] && std::strcmp(filter[i], name) == 0) return true;
  }
  return false;
}

void append_sv(DBusMessageIter* dict, const char* key, const char* sig, const auto& write_value) {
  DBusMessageIter ent;
  DBusMessageIter var;
  dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &ent);
  dbus_message_iter_append_basic(&ent, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&ent, DBUS_TYPE_VARIANT, sig, &var);
  write_value(&var);
  dbus_message_iter_close_container(&ent, &var);
  dbus_message_iter_close_container(dict, &ent);
}

void append_bool_prop(DBusMessageIter* dict, const char* key, bool value) {
  append_sv(dict, key, "b", [&](DBusMessageIter* var) {
    dbus_bool_t v = value ? TRUE : FALSE;
    dbus_message_iter_append_basic(var, DBUS_TYPE_BOOLEAN, &v);
  });
}

void append_string_prop(DBusMessageIter* dict, const char* key, const char* value) {
  append_sv(dict, key, "s", [&](DBusMessageIter* var) {
    dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &value);
  });
}

void write_item_props(DBusMessageIter* dict, int id, bool paused, char** filter, int nfilter) {
  auto want = [&](const char* n) { return name_wanted(n, filter, nfilter); };
  if (id == 0) {
    if (want("type")) append_string_prop(dict, "type", "standard");
    if (want("label")) append_string_prop(dict, "label", "AirMouse");
    if (want("enabled")) append_bool_prop(dict, "enabled", true);
    if (want("visible")) append_bool_prop(dict, "visible", true);
    if (want("children-display")) append_string_prop(dict, "children-display", "submenu");
    return;
  }
  const Item* item = find_item(id);
  if (!item) return;
  if (want("type")) append_string_prop(dict, "type", item->type);
  if (want("enabled")) append_bool_prop(dict, "enabled", true);
  if (want("visible")) append_bool_prop(dict, "visible", true);
  if (std::strcmp(item->type, "separator") == 0) return;
  if (want("label")) {
    const char* label = item->label;
    if (item->id == 1) label = paused ? "Resume" : "Pause";
    append_string_prop(dict, "label", label);
  }
}

void append_layout_node(DBusMessageIter* parent, int id, int depth, bool paused, char** filter,
                        int nfilter);

void append_children(DBusMessageIter* st, int id, int depth, bool paused, char** filter,
                     int nfilter) {
  DBusMessageIter kids;
  dbus_message_iter_open_container(st, DBUS_TYPE_ARRAY, "v", &kids);
  if (id == 0 && depth != 0) {
    const int next_depth = depth < 0 ? -1 : depth - 1;
    for (const auto& item : kStaticItems) {
      DBusMessageIter var;
      dbus_message_iter_open_container(&kids, DBUS_TYPE_VARIANT, "(ia{sv}av)", &var);
      append_layout_node(&var, item.id, next_depth, paused, filter, nfilter);
      dbus_message_iter_close_container(&kids, &var);
    }
  }
  dbus_message_iter_close_container(st, &kids);
}

void append_layout_node(DBusMessageIter* parent, int id, int depth, bool paused, char** filter,
                        int nfilter) {
  DBusMessageIter st;
  dbus_message_iter_open_container(parent, DBUS_TYPE_STRUCT, nullptr, &st);
  dbus_int32_t iid = id;
  dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &iid);
  DBusMessageIter dict;
  dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "{sv}", &dict);
  write_item_props(&dict, id, paused, filter, nfilter);
  dbus_message_iter_close_container(&st, &dict);
  append_children(&st, id, depth, paused, filter, nfilter);
  dbus_message_iter_close_container(parent, &st);
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
  <interface name="com.canonical.dbusmenu">
    <property name="Version" type="u" access="read"/>
    <property name="TextDirection" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="IconThemePath" type="as" access="read"/>
    <method name="GetLayout">
      <arg type="i" name="parentId" direction="in"/>
      <arg type="i" name="recursionDepth" direction="in"/>
      <arg type="as" name="propertyNames" direction="in"/>
      <arg type="u" name="revision" direction="out"/>
      <arg type="(ia{sv}av)" name="layout" direction="out"/>
    </method>
    <method name="GetGroupProperties">
      <arg type="ai" name="ids" direction="in"/>
      <arg type="as" name="propertyNames" direction="in"/>
      <arg type="a(ia{sv})" name="properties" direction="out"/>
    </method>
    <method name="GetProperty">
      <arg type="i" name="id" direction="in"/>
      <arg type="s" name="name" direction="in"/>
      <arg type="v" name="value" direction="out"/>
    </method>
    <method name="Event">
      <arg type="i" name="id" direction="in"/>
      <arg type="s" name="eventId" direction="in"/>
      <arg type="v" name="data" direction="in"/>
      <arg type="u" name="timestamp" direction="in"/>
    </method>
    <method name="EventGroup">
      <arg type="a(isvu)" name="events" direction="in"/>
      <arg type="ai" name="idErrors" direction="out"/>
    </method>
    <method name="AboutToShow">
      <arg type="i" name="id" direction="in"/>
      <arg type="b" name="needUpdate" direction="out"/>
    </method>
    <method name="AboutToShowGroup">
      <arg type="ai" name="ids" direction="in"/>
      <arg type="ai" name="updatesNeeded" direction="out"/>
      <arg type="ai" name="idErrors" direction="out"/>
    </method>
  </interface>
</node>
)xml";

}  // namespace

DbusMenu::~DbusMenu() { detach(); }

void DbusMenu::detach() {
  if (conn_ && registered_ && path_) {
    dbus_connection_unregister_object_path(conn_, path_);
    registered_ = false;
  }
}

bool DbusMenu::attach(DBusConnection* conn, const char* path) {
  if (!conn || !path) return false;
  conn_ = conn;
  path_ = path;
  DBusObjectPathVTable vtable{};
  vtable.message_function = &DbusMenu::on_message;
  DBusError err;
  dbus_error_init(&err);
  if (!dbus_connection_try_register_object_path(conn_, path_, &vtable, this, &err)) {
    if (dbus_error_is_set(&err)) dbus_error_free(&err);
    return false;
  }
  registered_ = true;
  return true;
}

void DbusMenu::set_handler(std::function<void(TrayAction)> handler) {
  handler_ = std::move(handler);
}

void DbusMenu::set_paused(bool paused) {
  if (paused_ == paused) return;
  paused_ = paused;
  ++revision_;
  if (!conn_ || !path_) return;
  DBusMessage* sig = dbus_message_new_signal(path_, kIface, "LayoutUpdated");
  if (!sig) return;
  dbus_uint32_t rev = revision_;
  dbus_int32_t parent = 0;
  dbus_message_append_args(sig, DBUS_TYPE_UINT32, &rev, DBUS_TYPE_INT32, &parent,
                           DBUS_TYPE_INVALID);
  dbus_connection_send(conn_, sig, nullptr);
  dbus_message_unref(sig);
}

DBusHandlerResult DbusMenu::on_message(DBusConnection*, DBusMessage* msg, void* data) {
  return static_cast<DbusMenu*>(data)->handle(msg);
}

DBusHandlerResult DbusMenu::handle(DBusMessage* msg) {
  if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
    reply_introspect(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
    reply_props_get(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll")) {
    reply_props_get_all(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Set")) {
    reply_void(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "GetLayout")) {
    reply_get_layout(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "GetGroupProperties")) {
    reply_get_group_properties(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "GetProperty")) {
    reply_get_property(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "AboutToShow")) {
    reply_about_to_show(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "AboutToShowGroup")) {
    reply_about_to_show_group(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "Event")) {
    reply_event(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(msg, kIface, "EventGroup")) {
    reply_event_group(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void DbusMenu::reply_void(DBusMessage* msg) {
  DBusMessage* reply = dbus_message_new_method_return(msg);
  if (reply) {
    dbus_connection_send(conn_, reply, nullptr);
    dbus_message_unref(reply);
  }
}

void DbusMenu::reply_introspect(DBusMessage* msg) {
  DBusMessage* reply = dbus_message_new_method_return(msg);
  const char* xml = kIntrospect;
  dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

void DbusMenu::reply_get_layout(DBusMessage* msg) {
  dbus_int32_t parent = 0;
  dbus_int32_t depth = -1;
  DBusMessageIter it;
  if (dbus_message_iter_init(msg, &it) && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_INT32) {
    dbus_message_iter_get_basic(&it, &parent);
    dbus_message_iter_next(&it);
    if (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_INT32) {
      dbus_message_iter_get_basic(&it, &depth);
      dbus_message_iter_next(&it);
    }
  }
  char** names = nullptr;
  int nnames = 0;
  DBusError err;
  dbus_error_init(&err);
  dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &parent, DBUS_TYPE_INT32, &depth,
                        DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &names, &nnames, DBUS_TYPE_INVALID);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    names = nullptr;
    nnames = 0;
  }

  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter rit;
  dbus_message_iter_init_append(reply, &rit);
  dbus_uint32_t rev = revision_;
  dbus_message_iter_append_basic(&rit, DBUS_TYPE_UINT32, &rev);
  append_layout_node(&rit, parent, depth, paused_, names, nnames);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
  if (names) dbus_free_string_array(names);
}

void DbusMenu::reply_get_group_properties(DBusMessage* msg) {
  dbus_int32_t* ids = nullptr;
  int nids = 0;
  char** names = nullptr;
  int nnames = 0;
  DBusError err;
  dbus_error_init(&err);
  dbus_message_get_args(msg, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &ids, &nids, DBUS_TYPE_ARRAY,
                        DBUS_TYPE_STRING, &names, &nnames, DBUS_TYPE_INVALID);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    ids = nullptr;
    nids = 0;
    names = nullptr;
    nnames = 0;
  }

  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter rit;
  DBusMessageIter arr;
  dbus_message_iter_init_append(reply, &rit);
  dbus_message_iter_open_container(&rit, DBUS_TYPE_ARRAY, "(ia{sv})", &arr);
  for (int i = 0; i < nids; ++i) {
    const int id = ids[i];
    if (id != 0 && !find_item(id)) continue;
    DBusMessageIter st;
    dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, nullptr, &st);
    dbus_int32_t iid = id;
    dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &iid);
    DBusMessageIter dict;
    dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "{sv}", &dict);
    write_item_props(&dict, id, paused_, names, nnames);
    dbus_message_iter_close_container(&st, &dict);
    dbus_message_iter_close_container(&arr, &st);
  }
  dbus_message_iter_close_container(&rit, &arr);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
  if (names) dbus_free_string_array(names);
}

void DbusMenu::reply_get_property(DBusMessage* msg) {
  dbus_int32_t id = 0;
  const char* name = "";
  dbus_message_get_args(msg, nullptr, DBUS_TYPE_INT32, &id, DBUS_TYPE_STRING, &name,
                        DBUS_TYPE_INVALID);
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter rit;
  dbus_message_iter_init_append(reply, &rit);
  const char* n = name ? name : "";
  if (std::strcmp(n, "enabled") == 0 || std::strcmp(n, "visible") == 0) {
    DBusMessageIter var;
    dbus_message_iter_open_container(&rit, DBUS_TYPE_VARIANT, "b", &var);
    dbus_bool_t v = TRUE;
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
    dbus_message_iter_close_container(&rit, &var);
  } else {
    const char* value = "";
    if (std::strcmp(n, "type") == 0) {
      if (id == 0) value = "standard";
      else if (const Item* item = find_item(id)) value = item->type;
    } else if (std::strcmp(n, "label") == 0) {
      if (id == 0) value = "AirMouse";
      else if (id == 1) value = paused_ ? "Resume" : "Pause";
      else if (const Item* item = find_item(id)) value = item->label ? item->label : "";
    } else if (std::strcmp(n, "children-display") == 0) {
      value = (id == 0) ? "submenu" : "";
    }
    DBusMessageIter var;
    dbus_message_iter_open_container(&rit, DBUS_TYPE_VARIANT, "s", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &value);
    dbus_message_iter_close_container(&rit, &var);
  }
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

void DbusMenu::reply_about_to_show(DBusMessage* msg) {
  DBusMessage* reply = dbus_message_new_method_return(msg);
  dbus_bool_t need = FALSE;
  dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &need, DBUS_TYPE_INVALID);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

void DbusMenu::reply_about_to_show_group(DBusMessage* msg) {
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter rit;
  DBusMessageIter a1;
  DBusMessageIter a2;
  dbus_message_iter_init_append(reply, &rit);
  dbus_message_iter_open_container(&rit, DBUS_TYPE_ARRAY, "i", &a1);
  dbus_message_iter_close_container(&rit, &a1);
  dbus_message_iter_open_container(&rit, DBUS_TYPE_ARRAY, "i", &a2);
  dbus_message_iter_close_container(&rit, &a2);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
  (void)msg;
}

void DbusMenu::handle_clicked(int id) {
  const Item* item = find_item(id);
  if (!item || !item->has_action || !handler_) return;
  handler_(item->action);
}

void DbusMenu::reply_event(DBusMessage* msg) {
  dbus_int32_t id = 0;
  const char* event_id = "";
  dbus_uint32_t ts = 0;
  DBusMessageIter it;
  if (dbus_message_iter_init(msg, &it) && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_INT32) {
    dbus_message_iter_get_basic(&it, &id);
    dbus_message_iter_next(&it);
    if (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&it, &event_id);
    }
  }
  (void)ts;
  if (event_id && std::strcmp(event_id, "clicked") == 0) {
    handle_clicked(id);
  }
  reply_void(msg);
}

void DbusMenu::reply_event_group(DBusMessage* msg) {
  DBusMessageIter it;
  if (dbus_message_iter_init(msg, &it) && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_ARRAY) {
    DBusMessageIter arr;
    dbus_message_iter_recurse(&it, &arr);
    while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRUCT) {
      DBusMessageIter st;
      dbus_message_iter_recurse(&arr, &st);
      dbus_int32_t id = 0;
      const char* event_id = "";
      if (dbus_message_iter_get_arg_type(&st) == DBUS_TYPE_INT32) {
        dbus_message_iter_get_basic(&st, &id);
        dbus_message_iter_next(&st);
      }
      if (dbus_message_iter_get_arg_type(&st) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&st, &event_id);
      }
      if (event_id && std::strcmp(event_id, "clicked") == 0) {
        handle_clicked(id);
      }
      dbus_message_iter_next(&arr);
    }
  }
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter rit;
  DBusMessageIter errs;
  dbus_message_iter_init_append(reply, &rit);
  dbus_message_iter_open_container(&rit, DBUS_TYPE_ARRAY, "i", &errs);
  dbus_message_iter_close_container(&rit, &errs);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

void DbusMenu::reply_props_get(DBusMessage* msg) {
  const char* iface = nullptr;
  const char* name = nullptr;
  dbus_message_get_args(msg, nullptr, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &name,
                        DBUS_TYPE_INVALID);
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter it;
  DBusMessageIter var;
  dbus_message_iter_init_append(reply, &it);
  const char* n = name ? name : "";
  if (std::strcmp(n, "Version") == 0) {
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, "u", &var);
    dbus_uint32_t v = 3;
    dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
    dbus_message_iter_close_container(&it, &var);
  } else if (std::strcmp(n, "TextDirection") == 0) {
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, "s", &var);
    const char* v = "ltr";
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
    dbus_message_iter_close_container(&it, &var);
  } else if (std::strcmp(n, "Status") == 0) {
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, "s", &var);
    const char* v = "normal";
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
    dbus_message_iter_close_container(&it, &var);
  } else {
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, "as", &var);
    DBusMessageIter arr;
    dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "s", &arr);
    dbus_message_iter_close_container(&var, &arr);
    dbus_message_iter_close_container(&it, &var);
  }
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

void DbusMenu::reply_props_get_all(DBusMessage* msg) {
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter it;
  DBusMessageIter dict;
  dbus_message_iter_init_append(reply, &it);
  dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &dict);
  append_sv(&dict, "Version", "u", [](DBusMessageIter* var) {
    dbus_uint32_t v = 3;
    dbus_message_iter_append_basic(var, DBUS_TYPE_UINT32, &v);
  });
  append_sv(&dict, "TextDirection", "s", [](DBusMessageIter* var) {
    const char* v = "ltr";
    dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
  });
  append_sv(&dict, "Status", "s", [](DBusMessageIter* var) {
    const char* v = "normal";
    dbus_message_iter_append_basic(var, DBUS_TYPE_STRING, &v);
  });
  append_sv(&dict, "IconThemePath", "as", [](DBusMessageIter* var) {
    DBusMessageIter arr;
    dbus_message_iter_open_container(var, DBUS_TYPE_ARRAY, "s", &arr);
    dbus_message_iter_close_container(var, &arr);
  });
  dbus_message_iter_close_container(&it, &dict);
  dbus_connection_send(conn_, reply, nullptr);
  dbus_message_unref(reply);
}

}  // namespace airmouse

#endif
