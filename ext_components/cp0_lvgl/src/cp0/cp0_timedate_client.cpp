#include "cp0_timedate_client.hpp"
#include <ctime>
#include <cstdio>
#include <string>
#if !defined(HAL_PLATFORM_SDL)
#include <gio/gio.h>
#include <sys/time.h>
#endif

#if defined(HAL_PLATFORM_SDL)
int cp0_timedate_get_ntp(int *) { return -1; }
int cp0_timedate_get_time(int[6]) { return -1; }
int cp0_timedate_set_ntp(int) { return -1; }
int cp0_timedate_set_time(const char *) { return -1; }
#else
namespace {
constexpr const char *bus = "org.freedesktop.timedate1";
constexpr const char *path = "/org/freedesktop/timedate1";
constexpr const char *iface = "org.freedesktop.timedate1";
GDBusConnection *connection(GError **e) { return g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, e); }
GVariant *get_property(GDBusConnection *c, const char *name, GError **e) {
    return g_dbus_connection_call_sync(c, bus, path, "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", iface, name), G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, e);
}

// Alpine fork: org.freedesktop.timedate1 is a systemd service that does not
// exist on OpenRC systems. Date/time reads and writes fall back to the
// process clock (localtime / settimeofday) whenever the dbus service is
// absent, so the Date & Time settings page keeps working; the RTC write
// itself remains the separate `hwclock -w` step driven by the page.
// The NTP toggle has no non-systemd equivalent and intentionally stays
// dbus-only: the settings page already reports it as unavailable when the
// property cannot be read.
bool read_system_time(int values[6]) {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    if (!localtime_r(&now, &tm)) return false;
    values[0] = tm.tm_year + 1900; values[1] = tm.tm_mon + 1; values[2] = tm.tm_mday;
    values[3] = tm.tm_hour; values[4] = tm.tm_min; values[5] = tm.tm_sec;
    return true;
}

bool write_system_time(std::time_t t) {
    timeval tv{};
    tv.tv_sec = t;
    return settimeofday(&tv, nullptr) == 0;
}
}
int cp0_timedate_get_ntp(int *enabled) {
    if (!enabled) return -22;
    GError *e = nullptr;
    auto *c = connection(&e);
    if (!c) {
        g_clear_error(&e);
        // Alpine/OpenRC has no systemd-timedated. cpz-ntp keeps the user's
        // choice in /cfg/ntp (persisted with the card) or /var/lib/cpz/ntp;
        // report it as available even before the first save, otherwise the
        // Date & Time page greys out both the NTP toggle and manual time.
        FILE *f = std::fopen("/cfg/ntp", "r");
        if (!f) f = std::fopen("/var/lib/cpz/ntp", "r");
        if (!f) {
            *enabled = 0;
            return 0;
        }
        int value = 0;
        const int read = std::fscanf(f, "%d", &value);
        std::fclose(f);
        if (read != 1) return -1;
        *enabled = value ? 1 : 0;
        return 0;
    }
    auto *r = get_property(c, "NTP", &e); GVariant *v = nullptr;
    if (r) g_variant_get(r, "(v)", &v);
    const bool ok = v && g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN);
    if (ok) *enabled = g_variant_get_boolean(v);
    if (v) g_variant_unref(v); if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c); return ok ? 0 : -1;
}
int cp0_timedate_get_time(int values[6]) {
    if (!values) return -22;
    {
        GError *e = nullptr; auto *c = connection(&e);
        if (c) {
            auto *r = get_property(c, "TimeUSec", &e); GVariant *v = nullptr; guint64 us = 0;
            if (r) g_variant_get(r, "(v)", &v);
            const bool ok = v && g_variant_is_of_type(v, G_VARIANT_TYPE_UINT64);
            if (ok) us = g_variant_get_uint64(v);
            if (v) g_variant_unref(v); if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c);
            if (ok) {
                std::time_t t = static_cast<std::time_t>(us / 1000000ULL); std::tm tm{};
                if (!localtime_r(&t, &tm)) return -1;
                values[0] = tm.tm_year + 1900; values[1] = tm.tm_mon + 1; values[2] = tm.tm_mday;
                values[3] = tm.tm_hour; values[4] = tm.tm_min; values[5] = tm.tm_sec; return 0;
            }
        } else {
            g_clear_error(&e);
        }
    }
    return read_system_time(values) ? 0 : -1;
}
int cp0_timedate_set_ntp(int enabled) {
    GError *e = nullptr; auto *c = connection(&e); if (!c) { g_clear_error(&e); return -1; }
    auto *r = g_dbus_connection_call_sync(c, bus, path, iface, "SetNTP", g_variant_new("(bb)", enabled, false), nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &e);
    const bool ok = r != nullptr; if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c); return ok ? 0 : -1;
}
int cp0_timedate_set_time(const char *timestamp) {
    if (!timestamp) return -22; const std::string s(timestamp);
    std::tm tm{}; if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return -22;
    tm.tm_year -= 1900; --tm.tm_mon; tm.tm_isdst = -1; const auto t = std::mktime(&tm); if (t < 0) return -22;
    {
        GError *e = nullptr; auto *c = connection(&e);
        if (c) {
            auto *r = g_dbus_connection_call_sync(c, bus, path, iface, "SetTime", g_variant_new("(xbb)", static_cast<gint64>(t) * 1000000, false, false), nullptr, G_DBUS_CALL_FLAGS_NONE, 30000, nullptr, &e);
            const bool ok = r != nullptr; if (r) g_variant_unref(r); g_clear_error(&e); g_object_unref(c);
            if (ok) return 0;
        } else {
            g_clear_error(&e);
        }
    }
    // No timedate1 service (OpenRC/Alpine): set the process clock directly.
    // Requires root (CAP_SYS_TIME); the launcher runs privileged on the
    // Alpine image, and a permission failure surfaces as a normal error.
    return write_system_time(t) ? 0 : -1;
}
#endif
