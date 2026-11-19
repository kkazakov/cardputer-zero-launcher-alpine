/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_audio_mixer_control.hpp"

#include "cp0_pulse_client.hpp"
#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <list>
#include <string>

// Alpine fork: the volume path used to be PulseAudio-only. The Alpine image
// runs plain ALSA (no pipewire-pulse), so both the Speaker settings page and
// the volume keys read/wrote nothing and the UI sat on a stale value. When
// libpulse is missing or serverless, fall back to the ES8389 hardware mixer
// through amixer: DACL/DACR are the card's 0-100% playback controls (the
// same pair cpz-audio.start restores at boot).
//
// All process access goes through the cp0 process signal (no direct
// fork/exec), matching the soundcard service's implementation.
namespace {

using arg_t = std::list<std::string>;

std::string capture_argv(std::initializer_list<const char *> args)
{
    arg_t req;
    req.push_back("CaptureArgv");
    for (const char *a : args)
        if (a) req.push_back(a);

    std::string out;
    cp0_signal_process_api(req, [&](int, std::string data) { out = std::move(data); });
    return out;
}

int run_argv(std::initializer_list<const char *> args)
{
    arg_t req;
    req.push_back("RunArgv");
    req.push_back("0");
    for (const char *a : args)
        if (a) req.push_back(a);

    int rc = -1;
    cp0_signal_process_api(req, [&](int code, std::string) { rc = code; });
    return rc;
}

// The playback control lives on the ES8389 simple-card (card 0 on the device;
// the vc4 HDMI card is card 1). Probe the first few card indexes so the
// fallback also works if the probe order changes.
int alsa_card_with_dac()
{
    static int cached = -2;
    if (cached != -2) return cached;
    for (int card = 0; card < 4; ++card) {
        const std::string num = std::to_string(card);
        const std::string out =
            capture_argv({"amixer", "-c", num.c_str(), "sget", "DACL"});
        if (out.find('%') != std::string::npos) {
            cached = card;
            return cached;
        }
    }
    cached = -1;
    return cached;
}

// Parse the first "[NN%]" token from `amixer sget DACL` output.
int parse_percent(const std::string &text)
{
    const std::size_t open = text.find('[');
    if (open == std::string::npos) return -1;
    const std::size_t close = text.find('%', open + 1);
    if (close == std::string::npos) return -1;
    const std::string digits = text.substr(open + 1, close - open - 1);
    char *end = nullptr;
    const long value = std::strtol(digits.c_str(), &end, 10);
    if (end == digits.c_str() || value < 0 || value > 100) return -1;
    return static_cast<int>(value);
}

int alsa_read_volume()
{
    const int card = alsa_card_with_dac();
    if (card < 0) return -1;
    const std::string num = std::to_string(card);
    return parse_percent(capture_argv({"amixer", "-c", num.c_str(), "sget", "DACL"}));
}

int alsa_write_volume(int percent)
{
    if (percent < 0) return -1;
    if (percent > 100) percent = 100;
    const int card = alsa_card_with_dac();
    if (card < 0) return -1;
    const std::string num = std::to_string(card);
    const std::string value = std::to_string(percent) + "%";
    const int left = run_argv({"amixer", "-c", num.c_str(), "sset", "DACL", value.c_str()});
    const int right = run_argv({"amixer", "-c", num.c_str(), "sset", "DACR", value.c_str()});
    return (left == 0 && right == 0) ? percent : -1;
}

} // namespace

std::string Cp0AudioMixerControl::default_sink_name() const
{
    std::string name;
    return cp0::pulse::default_sink_name(name) ? name : std::string();
}

int Cp0AudioMixerControl::read_volume() const
{
    const int pulse_volume = cp0::pulse::read_default_sink_volume();
    return pulse_volume >= 0 ? pulse_volume : alsa_read_volume();
}

int Cp0AudioMixerControl::write_volume(int percent) const
{
    const int pulse_written = cp0::pulse::write_default_sink_volume(percent);
    return pulse_written >= 0 ? pulse_written : alsa_write_volume(percent);
}

int Cp0AudioMixerControl::read_mute() const
{
    // The ALSA path has no mute control wired; report "unknown" so callers
    // keep the last known state instead of toggling blindly.
    return cp0::pulse::read_default_sink_mute();
}

int Cp0AudioMixerControl::toggle_mute() const
{
    return cp0::pulse::toggle_default_sink_mute();
}

bool Cp0AudioMixerControl::preload() const
{
    // Load libpulse when it exists; the ALSA fallback needs no preload (amixer
    // is resolved on first use). Returning pulse's result keeps the "Pulse
    // unavailable" diagnostic honest while read/write still work over ALSA.
    return cp0::pulse::preload();
}
