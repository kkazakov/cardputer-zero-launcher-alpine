/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_keyboard_keymap.h"

#include "input_keys.h"
#include "../../../../SDK/components/utilities/include/sample_log.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#define TCA8418_KEYMAP_PATH "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map"
#define TCA8418_KEYMAP_MAX_ENTRIES 64

struct runtime_keymap_entry {
    uint32_t keycode;
    char sym_name[64];
    char utf8[16];
};

/* Field fallback only. The shipped runtime map is
 * /usr/share/keymaps/tca8418_keypad_m5stack_keymap.map (0644); these are
 * the measured TCA8418 SYM-layer evdev codes, not the stock M5 table
 * (which used 26..93 and printed the wrong symbols on q..m). */
static const struct cp0_keyboard_keymap_entry default_keymap[] = {
    {183, "exclam", "!"},
    {184, "at", "@"},
    {185, "numbersign", "#"},
    {186, "dollar", "$"},
    {187, "percent", "%"},
    {188, "asciicircum", "^"},
    {189, "ampersand", "&"},
    {190, "asterisk", "*"},
    {191, "parenleft", "("},
    {192, "parenright", ")"},
    {193, "asciitilde", "~"},
    {194, "grave", "`"},
    {195, "underscore", "_"},
    {196, "minus", "-"},
    {197, "plus", "+"},
    {198, "equal", "="},
    {199, "bracketleft", "["},
    {200, "bracketright", "]"},
    {201, "braceleft", "{"},
    {202, "braceright", "}"},
    {231, "semicolon", ";"},
    {232, "colon", ":"},
    {233, "apostrophe", "'"},
    {209, "quotedbl", "\""},
    {210, "less", "<"},
    {211, "greater", ">"},
    {212, "backslash", "\\"},
    {213, "bar", "|"},
    {214, "comma", ","},
    {215, "period", "."},
    {216, "slash", "/"},
    {217, "question", "?"},
};

static struct runtime_keymap_entry runtime_keymap[TCA8418_KEYMAP_MAX_ENTRIES];
static size_t runtime_keymap_size;
static int runtime_keymap_loaded;

static char *trim_ascii(char *text)
{
    while (isspace((unsigned char)*text))
        ++text;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static int parse_keymap_line(char *line, struct runtime_keymap_entry *out)
{
    char *comment = strchr(line, '#');
    if (comment)
        *comment = '\0';

    char *body = trim_ascii(line);
    if (body[0] == '\0')
        return 0;

    uint32_t keycode = 0;
    char sym_name[sizeof(out->sym_name)] = {0};
    if (sscanf(body, "keycode %u = %63s", &keycode, sym_name) != 2)
        return 0;

    out->keycode = keycode;
    snprintf(out->sym_name, sizeof(out->sym_name), "%s", sym_name);
    xkb_keysym_t sym = xkb_keysym_from_name(out->sym_name, XKB_KEYSYM_NO_FLAGS);
    if (sym == XKB_KEY_NoSymbol ||
        xkb_keysym_to_utf8(sym, out->utf8, sizeof(out->utf8)) <= 0)
        out->utf8[0] = '\0';
    return 1;
}

void cp0_keyboard_keymap_load(void)
{
    if (access(TCA8418_KEYMAP_PATH, F_OK) != 0)
        return;

    FILE *file = fopen(TCA8418_KEYMAP_PATH, "r");
    if (!file) {
        SLOGW("[KBD] failed to open %s: %s; using built-in keymap",
              TCA8418_KEYMAP_PATH, strerror(errno));
        return;
    }

    char line[256];
    size_t count = 0;
    while (count < TCA8418_KEYMAP_MAX_ENTRIES && fgets(line, sizeof(line), file)) {
        struct runtime_keymap_entry entry = {0};
        if (parse_keymap_line(line, &entry))
            runtime_keymap[count++] = entry;
    }
    fclose(file);

    runtime_keymap_size = count;
    runtime_keymap_loaded = 1;
    SLOGI("[KBD] loaded %zu TCA8418 keymap entries from %s",
          runtime_keymap_size, TCA8418_KEYMAP_PATH);
}

const struct cp0_keyboard_keymap_entry *cp0_keyboard_keymap_lookup(uint32_t keycode)
{
    static struct cp0_keyboard_keymap_entry mapped;
    if (runtime_keymap_loaded) {
        for (size_t i = 0; i < runtime_keymap_size; ++i) {
            if (runtime_keymap[i].keycode == keycode) {
                mapped.keycode = runtime_keymap[i].keycode;
                mapped.sym_name = runtime_keymap[i].sym_name;
                mapped.utf8 = runtime_keymap[i].utf8;
                return &mapped;
            }
        }
        return NULL;
    }

    for (size_t i = 0; i < sizeof(default_keymap) / sizeof(default_keymap[0]); ++i)
        if (default_keymap[i].keycode == keycode)
            return &default_keymap[i];
    return NULL;
}
