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
            if (ve - vs >= XSETTINGS_PATH_MAX) { *why = "the path is over 255 bytes"; return line; }
            memcpy(dst, text + vs, ve - vs); dst[ve - vs] = 0;
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
