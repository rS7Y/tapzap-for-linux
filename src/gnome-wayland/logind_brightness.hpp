// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <cstdio>
#include <unistd.h>
#ifdef HAVE_LOGIND
#include <dbus/dbus.h>
#endif

namespace tapzap {
// Use the user's active seat session, including when the GUI runs as a user service.
class LogindBrightness {
public:
#ifdef HAVE_LOGIND
    ~LogindBrightness() { if (bus_) { dbus_connection_close(bus_); dbus_connection_unref(bus_); } }
    bool available() { return !activeSession().empty(); }
    bool set(const std::string& device, unsigned int value) {
        const auto session = activeSession();
        if (session.empty()) return false;
        auto* m = dbus_message_new_method_call("org.freedesktop.login1", session.c_str(),
            "org.freedesktop.login1.Session", "SetBrightness");
        if (!m) return false;
        const char* subsystem = "backlight";
        const char* name = device.c_str();
        dbus_uint32_t raw = value;
        dbus_message_append_args(m, DBUS_TYPE_STRING, &subsystem, DBUS_TYPE_STRING, &name,
                                 DBUS_TYPE_UINT32, &raw, DBUS_TYPE_INVALID);
        auto* r = call(m);
        if (!r) return false;
        dbus_message_unref(r);
        return true;
    }
private:
    DBusConnection* bus_ = nullptr;
    DBusMessage* call(DBusMessage* m) {
        if (!m) return nullptr;
        DBusError error; dbus_error_init(&error);
        if (!bus_) {
            bus_ = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error);
            if (bus_) dbus_connection_set_exit_on_disconnect(bus_, false);
        }
        DBusMessage* reply = bus_ ? dbus_connection_send_with_reply_and_block(bus_, m, 1000, &error) : nullptr;
        dbus_message_unref(m);
        if (dbus_error_is_set(&error)) {
            std::fprintf(stderr, "[PWM] logind: %s\n", error.message);
            dbus_error_free(&error);
        }
        return reply;
    }
    DBusMessage* property(const std::string& path, const char* interface, const char* property) {
        auto* m = dbus_message_new_method_call("org.freedesktop.login1", path.c_str(),
                                             "org.freedesktop.DBus.Properties", "Get");
        if (!m) return nullptr;
        dbus_message_append_args(m, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);
        return call(m);
    }
    std::string activeSession() {
        auto* m = dbus_message_new_method_call("org.freedesktop.login1", "/org/freedesktop/login1",
                                              "org.freedesktop.login1.Manager", "GetUser");
        if (!m) return {};
        dbus_uint32_t uid = getuid();
        dbus_message_append_args(m, DBUS_TYPE_UINT32, &uid, DBUS_TYPE_INVALID);
        auto* r = call(m);
        if (!r) return {};
        const char* userPath = nullptr;
        std::string path;
        if (dbus_message_get_args(r, nullptr, DBUS_TYPE_OBJECT_PATH, &userPath, DBUS_TYPE_INVALID)) path = userPath;
        dbus_message_unref(r);
        if (path.empty()) return {};
        r = property(path, "org.freedesktop.login1.User", "Display");
        if (!r) return {};
        DBusMessageIter outer, variant, fields;
        std::string session;
        if (dbus_message_iter_init(r, &outer) && dbus_message_iter_get_arg_type(&outer) == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&outer, &variant);
            if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRUCT) {
                dbus_message_iter_recurse(&variant, &fields);
                if (dbus_message_iter_next(&fields) && dbus_message_iter_get_arg_type(&fields) == DBUS_TYPE_OBJECT_PATH) {
                    const char* s = nullptr; dbus_message_iter_get_basic(&fields, &s); session = s;
                }
            }
        }
        dbus_message_unref(r);
        if (session.empty() || session == "/") return {};
        r = property(session, "org.freedesktop.login1.Session", "Active");
        if (!r) return {};
        dbus_bool_t active = false;
        if (dbus_message_iter_init(r, &outer) && dbus_message_iter_get_arg_type(&outer) == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&outer, &variant);
            if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN) dbus_message_iter_get_basic(&variant, &active);
        }
        dbus_message_unref(r);
        return active ? session : std::string{};
    }
#else
    bool available() { return false; }
    bool set(const std::string&, unsigned int) { return false; }
#endif
};
} // namespace tapzap
