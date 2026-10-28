#include "settings_edit.h"
#include <stdio.h>
#include <string.h>

enum { KEYS = 7 };
static const char *const keys[KEYS] = {
    "session_start", "session_command", "fastfetch", "fastfetch_look",
    "fastfetch_config", "fastfetch_hide", "fastfetch_logo"
};

/* The value of key k, or NULL when it is at its default. */
static const char *value(const XSettings *s, int k, char *buffer, size_t size)
{
    static const char *const start[] = { "off", "fastfetch", "command" };
    static const char *const look[] = { "compact", "picture", "file" };
    static const char *const logo[] = { "look", "text", "none" };
    size_t n = 0;
    int i;
    switch (k) {
    case 0: return s->session_start != XSTART_OFF && s->session_start <= XSTART_COMMAND ? start[s->session_start] : NULL;
    case 1: return s->session_command[0] ? s->session_command : NULL;
    case 2: return s->fastfetch[0] ? s->fastfetch : NULL;
    case 3: return s->fastfetch_look != XFF_COMPACT && s->fastfetch_look <= XFF_FILE ? look[s->fastfetch_look] : NULL;
    case 4: return s->fastfetch_config[0] ? s->fastfetch_config : NULL;
    case 5:
        if (!s->fastfetch_hide) return NULL;
        buffer[0] = 0;
        for (i = 0; i < XFF_GROUPS; ++i)
            if (s->fastfetch_hide & (1u << i))
                n += (size_t)snprintf(buffer + n, size - n, "%s%s", n ? "," : "", settings_group_names[i]);
        return buffer;
    default: return s->fastfetch_logo != XFF_LOGO_LOOK && s->fastfetch_logo <= XFF_LOGO_NONE ? logo[s->fastfetch_logo] : NULL;
    }
}

static int put(char *out, size_t capacity, size_t *n, const char *text, size_t length)
{
    if (*n + length >= capacity || *n + length > XSETTINGS_FILE_MAX) return 0;
    memcpy(out + *n, text, length);
    *n += length;
    out[*n] = 0;
    return 1;
}

static int put_key(char *out, size_t capacity, size_t *n, int k, const char *v)
{
    return put(out, capacity, n, keys[k], strlen(keys[k])) && put(out, capacity, n, "=", 1) &&
           put(out, capacity, n, v, strlen(v)) && put(out, capacity, n, "\n", 1);
}

/* Which session key a line sets, or -1 (comment, blank, any other key). */
static int line_key(const char *line, size_t length)
{
    size_t k = 0, e;
    int i;
    while (k < length && (line[k] == ' ' || line[k] == '\t')) ++k;
    e = k;
    while (e < length && line[e] != '=') ++e;
    if (e == length) return -1;
    while (e > k && (line[e - 1] == ' ' || line[e - 1] == '\t')) --e;
    for (i = 0; i < KEYS; ++i)
        if (strlen(keys[i]) == e - k && !memcmp(line + k, keys[i], e - k)) return i;
    return -1;
}

size_t settings_edit_session(const char *old, size_t old_length, const XSettings *s, char *out, size_t capacity)
{
    char buffer[64];
    unsigned written = 0;
    size_t pos = 0, n = 0;
    int k;
    if (!capacity) return 0;
    out[0] = 0;
    while (pos < old_length) {
        size_t end = pos;
        while (end < old_length && old[end] != '\n') ++end;
        k = line_key(old + pos, end - pos);
        if (k < 0) {
            /* As it was, with a line end if the file's last line had none. */
            if (!put(out, capacity, &n, old + pos, end - pos) || !put(out, capacity, &n, "\n", 1)) return 0;
        } else if (!(written & (1u << k))) {
            const char *v = value(s, k, buffer, sizeof buffer);
            written |= 1u << k;
            if (v && !put_key(out, capacity, &n, k, v)) return 0;
        }
        pos = end + 1;
    }
    for (k = 0; k < KEYS; ++k) {
        const char *v;
        if (written & (1u << k)) continue;
        v = value(s, k, buffer, sizeof buffer);
        if (v && !put_key(out, capacity, &n, k, v)) return 0;
    }
    return n;
}
