/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_hdmi.hpp"

#include "cp0_lvgl_app.h"
#include "launcher_platform.hpp"

#include <cstdio>
#include <cstring>
#include <list>
#include <string>

namespace {

constexpr uint32_t kBg = 0x0D1117;
constexpr uint32_t kTitleBg = 0x1F3A5F;
constexpr uint32_t kCardBg = 0x16202B;
constexpr uint32_t kCardBorder = 0x2A3F57;
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kMuted = 0x8FA3BF;
constexpr uint32_t kHint = 0x7EA8D8;
constexpr uint32_t kAccent = 0xF2C94C;
constexpr uint32_t kTrack = 0x24354A;

void run_background(const std::list<std::string> &argv)
{
    // RunArgv's second argument is the *background* flag parsed by
    // cp0_process_api_contract::parse_bool, which accepts only "0"/"1" —
    // "true" makes the whole command invalid and nothing runs.
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

lv_obj_t *make_label(lv_obj_t *parent, const char *text, uint32_t color,
                     const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

} // namespace

UIHdmiPage::UIHdmiPage() : AppPage()
{
    set_page_title("HDMI Desktop");
    last_desired_ = read_desired();
    last_running_ = desktop_running();
    build_ui();
    apply_ui_state(last_running_);
    poll_timer_ = lv_timer_create(poll_timer_cb, kPollMs, this);
}

UIHdmiPage::~UIHdmiPage()
{
    if (poll_timer_) {
        lv_timer_delete(poll_timer_);
        poll_timer_ = nullptr;
    }
}

void UIHdmiPage::build_ui()
{
    if (!ui_APP_Container) return;

    root_ = lv_obj_create(ui_APP_Container);
    if (!root_) return;
    lv_obj_set_size(root_, 320, 150);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // The page name comes from the global status bar (set_page_title), so
    // there is no second header inside the 320x150 content area.
    lv_obj_t *card = lv_obj_create(root_);
    if (!card) return;
    lv_obj_set_size(card, 296, 98);
    lv_obj_set_pos(card, 12, 6);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(kCardBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(kCardBorder), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_image_create(card);
    if (icon) {
        lv_image_set_src(icon, launcher_platform::path_c("hdmi_100.png"));
        lv_obj_set_size(icon, 44, 44);
        lv_image_set_scale(icon, 44 * 256 / 100);
        lv_obj_set_pos(icon, 14, 27);
    }

    lv_obj_t *name = make_label(card, "Desktop on HDMI", kText, &lv_font_montserrat_14);
    if (name) lv_obj_set_pos(name, 70, 16);

    // A real dot: LV_SYMBOL_BULLET is not part of the Montserrat subset
    // and rendered as a box glyph.
    status_dot_ = lv_obj_create(card);
    if (status_dot_) {
        lv_obj_set_size(status_dot_, 8, 8);
        lv_obj_set_pos(status_dot_, 70, 47);
        lv_obj_set_style_radius(status_dot_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(status_dot_, 255, LV_PART_MAIN);
        lv_obj_set_style_bg_color(status_dot_, lv_color_hex(kMuted), LV_PART_MAIN);
        lv_obj_set_style_border_width(status_dot_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(status_dot_, LV_OBJ_FLAG_SCROLLABLE);
    }
    status_label_ = make_label(card, "Off", kMuted, &lv_font_montserrat_12);
    if (status_label_) lv_obj_set_pos(status_label_, 84, 41);
    note_label_ = make_label(card, "HDMI stays dark while off", kMuted,
                             &lv_font_montserrat_10);
    if (note_label_) lv_obj_set_pos(note_label_, 70, 66);

    // LVGL 9.5 draws LV_PART_INDICATOR as the whole track (it does not
    // switch colour with LV_STATE_CHECKED), so the state-specific colours
    // go on that part, with the knob as a white circle on top.
    switch_ = lv_switch_create(card);
    if (switch_) {
        lv_obj_set_size(switch_, 56, 28);
        lv_obj_set_pos(switch_, 226, 40);
        lv_obj_set_style_radius(switch_, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(switch_, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(switch_, lv_color_hex(kTrack), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(switch_, lv_color_hex(kAccent),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(switch_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(switch_, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_radius(switch_, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_add_event_cb(switch_, switch_event_cb, LV_EVENT_VALUE_CHANGED, this);
    }

    lv_obj_t *hint = make_label(root_, "ENTER  toggle        ESC  back", kHint,
                                &lv_font_montserrat_10);
    if (hint) {
        lv_obj_set_align(hint, LV_ALIGN_TOP_MID);
        lv_obj_set_y(hint, 122);
    }

    // Keys arrive on the page screen (the input group's focus object), the
    // same place the IP panel handles its navigation keys.
    if (root_screen_) {
        lv_obj_add_event_cb(root_screen_, key_event_cb, LV_EVENT_KEY, this);
    }
}

void UIHdmiPage::apply_ui_state(bool running)
{
    if (switch_) {
        if (running) {
            lv_obj_add_state(switch_, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(switch_, LV_STATE_CHECKED);
        }
    }
    if (status_label_)
        lv_label_set_text(status_label_, running ? "On - session running" : "Off - idle");
    if (note_label_) {
        lv_label_set_text(note_label_, running ? "Sway + waybar on the HDMI output"
                                               : "HDMI stays dark while off");
    }
    if (status_dot_)
        lv_obj_set_style_bg_color(status_dot_,
                                  lv_color_hex(running ? 0x4CD964 : kMuted),
                                  LV_PART_MAIN);
    if (status_label_)
        lv_obj_set_style_text_color(status_label_,
                                    lv_color_hex(running ? 0xDFF5E1 : kMuted),
                                    LV_PART_MAIN);
}

void UIHdmiPage::set_desired(bool on, bool apply_now)
{
    last_desired_ = on;
    write_desired(on);
    if (apply_now) request_desktop(on);
}

bool UIHdmiPage::read_desired() const
{
    char line[8] = {0};
    if (cp0_file_read_first_line(kStatePath, line, sizeof(line)) <= 0) return false;
    return line[0] == '1';
}

void UIHdmiPage::write_desired(bool on) const
{
    run_background({"/bin/sh", "-c",
                    std::string("printf '%s\\n' ") + (on ? "1" : "0") +
                        " > " + kStatePath});
}

bool UIHdmiPage::desktop_running() const
{
    return !capture({"pgrep", "-x", "sway"}).empty();
}

void UIHdmiPage::request_desktop(bool on) const
{
    // cpz owns the session; cpz-desktop escalates only the seatd/runtime-dir
    // steps through sudo (NOPASSWD on this image).
    run_background({"sudo", "-n", "/usr/local/bin/cpz-desktop", on ? "start" : "stop"});
}

void UIHdmiPage::poll_timer_cb(lv_timer_t *timer)
{
    auto *self = static_cast<UIHdmiPage *>(lv_timer_get_user_data(timer));
    if (!self) return;
    const bool running = self->desktop_running();
    if (running != self->last_running_) {
        self->last_running_ = running;
        self->pending_ = false;
        self->apply_ui_state(running);
        return;
    }
    if (self->pending_ && lv_tick_elaps(self->pending_at_) > 12000) {
        // The request produced no session change: say so instead of
        // leaving "Starting..." on screen forever, and re-sync the switch
        // and the boot preference to the real state.
        self->pending_ = false;
        self->set_desired(running, false);
        self->apply_ui_state(running);
        if (self->status_label_)
            lv_label_set_text(self->status_label_,
                              "No change - see application log");
    }
}

void UIHdmiPage::switch_event_cb(lv_event_t *event)
{
    auto *self = static_cast<UIHdmiPage *>(lv_event_get_user_data(event));
    if (!self || !self->switch_) return;
    const bool on = lv_obj_has_state(self->switch_, LV_STATE_CHECKED);
    self->set_desired(on, true);
    self->pending_ = true;
    self->pending_at_ = lv_tick_get();
    if (self->status_label_)
        lv_label_set_text(self->status_label_, on ? "Starting..." : "Stopping...");
}

void UIHdmiPage::key_event_cb(lv_event_t *event)
{
    auto *self = static_cast<UIHdmiPage *>(lv_event_get_user_data(event));
    if (!self) return;
    if (lv_event_get_code(event) != LV_EVENT_KEY) return;
    const uint32_t key = lv_event_get_key(event);
    switch (key) {
    case LV_KEY_ESC:
        if (self->navigate_home) self->navigate_home();
        break;
    case LV_KEY_ENTER:
        if (self->switch_) {
            // Toggle through the widget so VALUE_CHANGED drives the same
            // path the UI uses; the switch itself is not in a group.
            if (lv_obj_has_state(self->switch_, LV_STATE_CHECKED))
                lv_obj_remove_state(self->switch_, LV_STATE_CHECKED);
            else
                lv_obj_add_state(self->switch_, LV_STATE_CHECKED);
            lv_obj_send_event(self->switch_, LV_EVENT_VALUE_CHANGED, nullptr);
        }
        break;
    default:
        break;
    }
}
