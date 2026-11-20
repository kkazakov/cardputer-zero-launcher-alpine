/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Global HDMI-session lock.
 *
 * When sway appears, wait a short grace period, then pause the launcher's
 * own keypad input and show "HDMI LOCKED" on the LCD. The internal keypad
 * belongs to the HDMI session until the session ends. Hold ESC for 2 s to
 * stop the session and get the LCD back; the same cleanup runs when sway
 * exits on its own / via Mod+Shift+E / via an external stop.
 */
void ui_hdmi_lock_init(void);
void ui_hdmi_lock_deinit(void);

#ifdef __cplusplus
}
#endif