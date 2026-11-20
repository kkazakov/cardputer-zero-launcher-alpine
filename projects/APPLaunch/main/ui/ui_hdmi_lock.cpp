/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_hdmi_lock.h"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "sample_log.h"
#include "ui_screensaver.h"

#include "lvgl/lvgl.h"

#include <cstdio>
#include <list>
#include <string>

namespace {

// 1 s: the ENTER release that started the HDMI session must not be
// interpreted by the freshly started compositor, and a session that dies
// immediately must not flash the lock screen.
constexpr uint32_t kLockGraceMs = 1000;
// Poll pgrep this often; the HDMI page also polls at 700 ms but this module
// must react even when the page is not on screen (boot hook / SSH start).
constexpr uint32_t kSwayPollMs = 500;
// Give cpz-desktop stop time to kill sway before escalating.
constexpr uint32_t kStopEscalateMs = 4000;
constexpr uint32_t kStopGiveUpMs = 8000;
// Must match the user-facing hint and the platform watcher's intent.
constexpr uint32_t kEscHoldMs = 2000;

enum class LockState {
    Idle,
    Grace,
    Locked,
    Unlocking,
};

LockState s_state = LockState::Idle;
uint32_t s_grace_until = 0;
uint32_t s_last_sway_poll = 0;
uint32_t s_unlock_started = 0;
bool s_sway_running = false;
bool s_stop_sent = false;
bool s_kill_sent = false;
bool s_initialized = false;
lv_timer_t *s_timer = nullptr;
lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_hint = nullptr;
lv_obj_t *s_detail = nullptr;

void run_background(const std::list<std::string> &argv)
{
    std::list<std::string> request = {"RunArgv", "1"};
    request.insert(request.end(), argv.begin(), argv.end());
    cp0_signal_process_api(request, [](int, std::string) {});
}

std::string capture(const std::list<std::string> &argv)
{
    std::list<std::string> request = {"CaptureArgv"};
    request.insert(request.end(), argv.begin(), argv.end());
    std::string out;
    cp0_signal_process_api(request, [&](int, std::string data) { out = std::move(data); });
    return out;
}

bool sway_running()
{
    return !capture({"pgrep", "-x", "sway"}).empty();
}

void write_preference_off()
{
    // Same state file and same "off" action as the HDMI page. The next boot
    // hook reads 0 and leaves the HDMI output dark.
    run_background({"/bin/sh", "-c",
                    "printf '0\\n' > /var/lib/applaunch/hdmi-desktop"});
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, uint32_t color,
                     const lv_font_t *font, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 300);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

void ensure_overlay()
{
    if (s_overlay) return;

    s_overlay = lv_obj_create(lv_layer_top());
    if (!s_overlay) return;
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_overlay, lv_display_get_horizontal_resolution(nullptr),
                    lv_display_get_vertical_resolution(nullptr));
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x0B0F14), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);

    make_label(s_overlay, "HDMI LOCKED", 0xFFFFFF, &lv_font_montserrat_14, 38);
    make_label(s_overlay, "Keyboard is with the HDMI desktop", 0x8A9BB0,
               &lv_font_montserrat_12, 68);
    s_hint = make_label(s_overlay, "Hold ESC 2s to stop HDMI", 0xF2C94C,
                        &lv_font_montserrat_12, 104);
    s_detail = make_label(s_overlay, "Mod+Shift+E also stops the desktop", 0x6F8FAF,
                          &lv_font_montserrat_12, 128);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void set_text(lv_obj_t *label, const char *text)
{
    if (label) lv_label_set_text(label, text);
}

void show_overlay()
{
    ensure_overlay();
    if (!s_overlay) return;
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

void hide_overlay()
{
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void enter_lock()
{
    keyboard_pause();
    ui_screensaver_set_foreground(0);
    show_overlay();
    set_text(s_hint, "Hold ESC 2s to stop HDMI");
    set_text(s_detail, "Mod+Shift+E also stops the desktop");
    cp0_keyboard_esc_hold_watch_start();
    s_state = LockState::Locked;
    s_stop_sent = false;
    s_kill_sent = false;
    SLOGI("[HDMI] LCD locked; hold ESC 2s to stop the desktop");
}

void cleanup_ui(bool send_stop, bool persist_off)
{
    cp0_keyboard_esc_hold_watch_stop();
    if (send_stop)
        run_background({"sudo", "-n", "/usr/local/bin/cpz-desktop", "stop"});
    keyboard_resume();
    ui_screensaver_set_foreground(1);
    hide_overlay();
    if (persist_off) write_preference_off();
    s_state = LockState::Idle;
    s_stop_sent = false;
    s_kill_sent = false;
    SLOGI("[HDMI] LCD unlocked%s", persist_off ? " (preference set to 0)" : "");
}

void begin_stop()
{
    if (s_stop_sent) return;
    s_stop_sent = true;
    s_state = LockState::Unlocking;
    s_unlock_started = lv_tick_get();
    set_text(s_hint, "Stopping HDMI...");
    set_text(s_detail, "The LCD will return shortly");
    run_background({"sudo", "-n", "/usr/local/bin/cpz-desktop", "stop"});
    SLOGI("[HDMI] ESC hold reached 2s; stopping the desktop");
}

void update_locked()
{
    const uint32_t held = cp0_keyboard_esc_held_ms();
    if (held >= kEscHoldMs) {
        begin_stop();
        return;
    }
    if (held > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fs / 2.0s", held / 1000.0);
        set_text(s_hint, "Keep holding ESC...");
        set_text(s_detail, buf);
    } else {
        set_text(s_hint, "Hold ESC 2s to stop HDMI");
        set_text(s_detail, "Mod+Shift+E also stops the desktop");
    }
}

void timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (lv_tick_elaps(s_last_sway_poll) >= kSwayPollMs) {
        s_last_sway_poll = lv_tick_get();
        s_sway_running = sway_running();
    }

    switch (s_state) {
    case LockState::Idle:
        if (s_sway_running) {
            s_state = LockState::Grace;
            s_grace_until = lv_tick_get() + kLockGraceMs;
            SLOGI("[HDMI] sway detected; locking the LCD in 1s");
        }
        break;

    case LockState::Grace:
        if (!s_sway_running) {
            s_state = LockState::Idle;
        } else if (static_cast<int32_t>(lv_tick_get() - s_grace_until) >= 0) {
            enter_lock();
        }
        break;

    case LockState::Locked:
        if (!s_sway_running) {
            cleanup_ui(true, true);
        } else {
            update_locked();
        }
        break;

    case LockState::Unlocking:
        if (!s_sway_running) {
            cleanup_ui(false, true);
            break;
        }
        if (!s_kill_sent && lv_tick_elaps(s_unlock_started) >= kStopEscalateMs) {
            s_kill_sent = true;
            run_background({"pkill", "-9", "-x", "sway"});
            SLOGI("[HDMI] stop timed out; sent SIGKILL to sway");
        }
        if (lv_tick_elaps(s_unlock_started) >= kStopGiveUpMs) {
            SLOGE("[HDMI] sway still running after stop timeout; unlocking anyway");
            cleanup_ui(false, true);
        }
        break;
    }
}

} // namespace

extern "C" void ui_hdmi_lock_init(void)
{
    if (s_initialized) return;
    s_initialized = true;
    s_state = LockState::Idle;
    s_last_sway_poll = lv_tick_get();
    s_timer = lv_timer_create(timer_cb, 250, nullptr);
    SLOGI("[HDMI] lock monitor started");
}

extern "C" void ui_hdmi_lock_deinit(void)
{
    if (!s_initialized) return;
    s_initialized = false;
    cp0_keyboard_esc_hold_watch_stop();
    if (s_state == LockState::Locked || s_state == LockState::Unlocking) {
        // Launcher teardown must not kill a healthy desktop session; just
        // hand the keypad back. A restarted launcher will re-lock it.
        keyboard_resume();
        ui_screensaver_set_foreground(1);
    }
    s_state = LockState::Idle;
    hide_overlay();
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = nullptr;
    }
    if (s_overlay) {
        lv_obj_delete(s_overlay);
        s_overlay = nullptr;
        s_hint = nullptr;
        s_detail = nullptr;
    }
}