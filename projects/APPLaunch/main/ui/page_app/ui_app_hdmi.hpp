/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../launcher_ui_app_page.hpp"

// ============================================================
//  HDMI desktop toggle  UIHdmiPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  Alpine fork page. Toggles the on-demand HDMI session (`cpz-desktop
//  start|stop`): the panel normally stays dark, and starting the desktop
//  brings sway + waybar up on HDMI. The choice is persisted in
//  /var/lib/applaunch/hdmi-desktop and re-applied at boot by
//  /etc/local.d/cpz-hdmi-boot.start.
//
//  UI contract: one switch, one status line, one hint line. ENTER/SYM?
//  toggles, ESC returns home. State is polled from the real session
//  (sway) so the switch never lies after an external start/stop.
// ============================================================

class UIHdmiPage : public AppPage
{
public:
    UIHdmiPage();
    ~UIHdmiPage() override;

private:
    static constexpr const char *kStatePath = "/var/lib/applaunch/hdmi-desktop";
    static constexpr int kTitleH = 22;
    static constexpr int kPollMs = 700;

    lv_obj_t *root_ = nullptr;
    lv_obj_t *switch_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *status_dot_ = nullptr;
    lv_timer_t *poll_timer_ = nullptr;
    bool last_desired_ = false;
    bool last_running_ = false;

    void build_ui();
    void apply_ui_state(bool running);
    void set_desired(bool on, bool apply_now);
    bool read_desired() const;
    void write_desired(bool on) const;
    bool desktop_running() const;
    void request_desktop(bool on) const;

    static void poll_timer_cb(lv_timer_t *timer);
    static void switch_event_cb(lv_event_t *event);
    static void key_event_cb(lv_event_t *event);
};
