/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "package_status_watch.hpp"

#include "launcher_toast.h"
#include "cp0_lvgl_app.h"
#include "lvgl.h"

#include <cstring>

namespace {

lv_timer_t *g_timer = nullptr;
uint32_t g_elapsed_ms = 0;
bool g_announced = false;

void stop_timer()
{
    if (g_timer) {
        lv_timer_delete(g_timer);
        g_timer = nullptr;
    }
}

void tick(lv_timer_t *)
{
    // cpz-wifi.start starts after the launcher; keep watching for 5 min so a
    // slow Wi-Fi association still gets announced.
    g_elapsed_ms += 2000;
    if (g_elapsed_ms > 5 * 60 * 1000) {
        stop_timer();
        return;
    }

    char line[32] = {};
    if (cp0_file_read_first_line("/run/cpz-pkg.status", line, sizeof(line)) != 0) return;

    if (!g_announced &&
        (std::strncmp(line, "pending", 7) == 0 || std::strncmp(line, "installing", 10) == 0)) {
        launcher_toast().show("Installing packages...");
        g_announced = true;
        return;
    }
    if (std::strncmp(line, "ok", 2) == 0) {
        if (g_announced) launcher_toast().show("Packages ready");
        stop_timer();
    } else if (std::strncmp(line, "failed", 6) == 0) {
        launcher_toast().show("Package install failed");
        stop_timer();
    }
}

} // namespace

void launcher_package_status_start()
{
    if (!g_timer) g_timer = lv_timer_create(tick, 2000, nullptr);
}

void launcher_package_status_stop()
{
    stop_timer();
}
