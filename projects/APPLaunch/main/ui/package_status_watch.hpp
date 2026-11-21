/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Shows a toast while cpz-wifi.start is reinstalling the archived package
// list (tailscale, ...) and a final "Packages ready"/"failed" toast. The
// install runs in the background after boot, so the launcher polls the
// status file the hook writes.
void launcher_package_status_start();
void launcher_package_status_stop();
