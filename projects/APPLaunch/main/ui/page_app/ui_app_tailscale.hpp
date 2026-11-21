/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../launcher_ui_app_page.hpp"

#include <cstdint>

// ============================================================
//  Tailscale tunnel toggle  UITailscalePage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  Alpine fork page. Toggles the optional Tailscale daemon
//  (`/etc/init.d/tailscale`, service name "tailscale" — not
//  "tailscaled"). Tailscale is NOT part of the image: it is an on-device
//  `apk add tailscale tailscale-openrc` install, and the home entry is
//  hidden by a runtime availability predicate until the package exists
//  and the node has been logged in (see builtin_app_registry.cpp).
//
//  The choice is persisted in /var/lib/applaunch/tailscale and re-applied
//  at boot by /etc/local.d/cpz-tailscale-boot.start. `apply` (rather than
//  `on`/`off`) is deliberate: the page owns the preference file, so two
//  fast toggles cannot race a stale helper write into it.
//
//  UI contract: one switch, one status line, one hint line. ENTER toggles,
//  ESC returns home. State is polled from the real daemon (tailscaled) so
//  the switch never lies after an external start/stop.
// ============================================================

class UITailscalePage : public AppPage
{
public:
    UITailscalePage();
    ~UITailscalePage() override;

private:
    static constexpr const char *kStatePath = "/var/lib/applaunch/tailscale";
    static constexpr int kPollMs = 700;

    lv_obj_t *root_ = nullptr;
    lv_obj_t *switch_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *status_dot_ = nullptr;
    lv_obj_t *note_label_ = nullptr;
    lv_timer_t *poll_timer_ = nullptr;
    bool last_desired_ = false;
    bool last_running_ = false;
    // A start/stop request is in flight: the status text says
    // "Starting.../Stopping..." and a timeout falls back to the real state
    // instead of lying forever.
    bool pending_ = false;
    uint32_t pending_at_ = 0;

    void build_ui();
    void apply_ui_state(bool running);
    void set_desired(bool on, bool apply_now);
    bool read_desired() const;
    void write_desired(bool on) const;
    bool tunnel_running() const;
    void request_tunnel(bool on) const;

    static void poll_timer_cb(lv_timer_t *timer);
    static void switch_event_cb(lv_event_t *event);
    static void key_event_cb(lv_event_t *event);
};
