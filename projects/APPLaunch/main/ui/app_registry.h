/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

enum class LauncherAppOrigin : std::uint8_t {
    Builtin,
    Preinstalled,
    StoreInstalled,
};

// Optional runtime gate for a built-in entry. Returning false hides the app
// from the home carousel and from Settings -> Launcher without touching its
// saved enabled flag (so it comes back with the same state when the
// precondition returns). Called from the LVGL thread only.
typedef bool (*LauncherAppAvailabilityFn)();

struct AppDescriptor {
    const char *label;
    const char *icon;
    const char *config_key;
    bool configurable;
    bool always_on;
    LauncherAppOrigin origin = LauncherAppOrigin::Builtin;
    // Null means "always available". The Tailscale page uses this to stay
    // hidden until the daemon is installed AND the node is logged in.
    LauncherAppAvailabilityFn available = nullptr;
};

inline bool launcher_app_registry_is_available(const AppDescriptor &desc)
{
    return !desc.available || desc.available();
}

const AppDescriptor *launcher_app_registry_entries(std::size_t *count);
bool launcher_app_registry_is_enabled(const AppDescriptor &desc);
bool launcher_app_registry_set_enabled(const AppDescriptor &desc, bool enabled);

typedef void (*LauncherAppRegistryChangedCallback)(void *user_data);
void launcher_app_registry_set_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                void *user_data);
bool launcher_app_registry_clear_changed_callback(LauncherAppRegistryChangedCallback callback,
                                                  void *user_data);
void launcher_app_registry_notify_changed();
