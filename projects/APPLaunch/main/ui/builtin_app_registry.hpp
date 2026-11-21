/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>

#include "app_registry.h"

struct app;

void launcher_append_enabled_builtin_apps(std::list<app> &apps);
// Bit per conditional built-in entry (table order). The home screen polls
// this so an app whose precondition just became true (Tailscale installed
// and logged in) appears without restarting the launcher.
std::uint32_t launcher_builtin_app_availability_mask();
void launcher_sort_app_display_order(std::list<app> &apps);
bool launcher_builtin_app_owns_exec(const std::string &exec);
const AppDescriptor *launcher_builtin_app_registry_entries(std::size_t *count);

void launcher_app_registry_begin_dynamic_refresh();
void launcher_app_registry_commit_dynamic_refresh();
void launcher_app_registry_cancel_dynamic_refresh();
bool launcher_app_registry_register_dynamic(const std::string &label,
                                            const std::string &icon,
                                            const std::string &config_key,
                                            LauncherAppOrigin origin);
