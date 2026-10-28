#include "settings.h"
#include <string.h>

void settings_default(XSettings *s)
{
    memset(s, 0, sizeof *s);
    s->font_size = XSETTINGS_FONT_SIZE_DEFAULT;
    s->scrollback = XSETTINGS_SCROLLBACK_DEFAULT;
    s->history = XSETTINGS_HISTORY_DEFAULT;
}

static int number(const char *v, size_t n, unsigned low, unsigned high, unsigned *out)
{
    unsigned long value = 0; size_t i;
    if (!n || n > 6) return 0;
    for (i = 0; i < n; ++i) {
        if (v[i] < '0' || v[i] > '9') return 0;
        value = value * 10 + (unsigned)(v[i] - '0');
    }
    if (value < low || value > high) return 0;
    *out = (unsigned)value;
    return 1;
}

static int is_space(char c) { return c == ' ' || c == '\t'; }

const char *const settings_group_names[XFF_GROUPS] = { "system", "hardware", "memory-disks", "network" };

/* One of the words, in order; -1 for anything else. */
static int word(const char *v, size_t n, const char *const *words, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        if (strlen(words[i]) == n && !memcmp(v, words[i], n)) return i;
    return -1;
}

/* A comma-separated list of group names, spaces allowed around them; empty
 * is none. */
static int groups(const char *v, size_t n, unsigned *out)
{
    unsigned bits = 0;
    size_t i = 0;
    while (i < n) {
        size_t s, e;
        int g;
        while (i < n && is_space(v[i])) ++i;
        s = i;
        while (i < n && v[i] != ',') ++i;
        e = i;
        while (e > s && is_space(v[e - 1])) --e;
        if (e == s) return 0;
        g = word(v + s, e - s, settings_group_names, XFF_GROUPS);
        if (g < 0 || (bits & (1u << g))) return 0;
        bits |= 1u << g;
        if (i < n) { ++i; if (i == n) return 0; }
    }
    *out = bits;
    return 1;
}

static int text_value(char *dst, size_t capacity, const char *v, size_t n, const char **why)
{
    if (!n) { *why = "the value is empty"; return 0; }
    if (n >= capacity) {
        *why = capacity == XSETTINGS_PATH_MAX ? "the path is over 255 bytes" : "the command is over 399 bytes";
        return 0;
    }
    memcpy(dst, v, n); dst[n] = 0;
    return 1;
}

int settings_parse(XSettings *out, const char *text, size_t length, const char **why)
{
    XSettings s = *out;
    size_t pos = 0;
    int line = 0;
    unsigned given = 0;
    *why = NULL;
    if (length > XSETTINGS_FILE_MAX) { *why = "file is over 4096 bytes"; return 1; }
    for (pos = 0; pos < length; ++pos) {
        unsigned char c = (unsigned char)text[pos];
        if (!c || (c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127) {
            *why = "not a text file"; return 1;
        }
    }
    pos = 0;
    while (pos < length) {
        size_t end = pos, k, ke, vs, ve;
        unsigned bit = 0;
        ++line;
        while (end < length && text[end] != '\n') ++end;
        ve = end;
        if (ve > pos && text[ve - 1] == '\r') --ve;
        k = pos;
        while (k < ve && is_space(text[k])) ++k;
        if (k == ve || text[k] == '#') { pos = end + 1; continue; }
        ke = k;
        while (ke < ve && text[ke] != '=') ++ke;
        if (ke == ve) { *why = "no '=' on the line"; return line; }
        vs = ke + 1;
        while (ke > k && is_space(text[ke - 1])) --ke;
        while (vs < ve && is_space(text[vs])) ++vs;
        while (ve > vs && is_space(text[ve - 1])) --ve;
#define KEY(name) (ke - k == sizeof name - 1 && !memcmp(text + k, name, sizeof name - 1))
        if (KEY("font") || KEY("theme")) {
            char *dst = KEY("font") ? s.font : s.theme;
            bit = KEY("font") ? XSET_FONT : XSET_THEME;
            if (vs == ve) { *why = "the path is empty"; return line; }
            if (!text_value(dst, XSETTINGS_PATH_MAX, text + vs, ve - vs, why)) return line;
        } else if (KEY("font_size")) {
            bit = XSET_FONT_SIZE;
            if (!number(text + vs, ve - vs, XSETTINGS_FONT_SIZE_MIN, XSETTINGS_FONT_SIZE_MAX, &s.font_size)) {
                *why = "font_size takes 8 to 20"; return line;
            }
        } else if (KEY("scrollback")) {
            bit = XSET_SCROLLBACK;
            if (!number(text + vs, ve - vs, XSETTINGS_SCROLLBACK_MIN, XSETTINGS_SCROLLBACK_MAX, &s.scrollback)) {
                *why = "scrollback takes 100 to 1000"; return line;
            }
        } else if (KEY("history")) {
            bit = XSET_HISTORY;
            if (!number(text + vs, ve - vs, XSETTINGS_HISTORY_MIN, XSETTINGS_HISTORY_MAX, &s.history)) {
                *why = "history takes 1 to 500"; return line;
            }
        } else if (KEY("session_start")) {
            static const char *const w[] = { "off", "fastfetch", "command" };
            int v = word(text + vs, ve - vs, w, 3);
            bit = XSET_SESSION_START;
            if (v < 0) { *why = "session_start takes off, fastfetch or command"; return line; }
            s.session_start = (unsigned)v;
        } else if (KEY("session_command")) {
            bit = XSET_SESSION_COMMAND;
            if (!text_value(s.session_command, sizeof s.session_command, text + vs, ve - vs, why)) return line;
        } else if (KEY("fastfetch")) {
            bit = XSET_FASTFETCH;
            if (!text_value(s.fastfetch, sizeof s.fastfetch, text + vs, ve - vs, why)) return line;
        } else if (KEY("fastfetch_look")) {
            static const char *const w[] = { "compact", "picture", "file" };
            int v = word(text + vs, ve - vs, w, 3);
            bit = XSET_FASTFETCH_LOOK;
            if (v < 0) { *why = "fastfetch_look takes compact, picture or file"; return line; }
            s.fastfetch_look = (unsigned)v;
        } else if (KEY("fastfetch_config")) {
            bit = XSET_FASTFETCH_CONFIG;
            if (!text_value(s.fastfetch_config, sizeof s.fastfetch_config, text + vs, ve - vs, why)) return line;
        } else if (KEY("fastfetch_hide")) {
            bit = XSET_FASTFETCH_HIDE;
            if (!groups(text + vs, ve - vs, &s.fastfetch_hide)) {
                *why = "fastfetch_hide takes system, hardware, memory-disks and network, separated by commas";
                return line;
            }
        } else if (KEY("fastfetch_logo")) {
            static const char *const w[] = { "look", "text", "none" };
            int v = word(text + vs, ve - vs, w, 3);
            bit = XSET_FASTFETCH_LOGO;
            if (v < 0) { *why = "fastfetch_logo takes look, text or none"; return line; }
            s.fastfetch_logo = (unsigned)v;
        } else { *why = "unknown key"; return line; }
#undef KEY
        if (given & bit) { *why = "the key is there twice"; return line; }
        given |= bit;
        pos = end + 1;
    }
    s.given = given;
    *out = s;
    return 0;
}
