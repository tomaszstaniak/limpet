#include "console_name.h"
#include <string.h>

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* strnicmp(field, keyword, n) == 0, as CON: calls it: n is the field's
 * length, so the field must be a prefix of the keyword (or the keyword
 * itself); a longer field meets the keyword's NUL and differs. */
static int prefix(const char *field, size_t n, const char *keyword)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        if (!keyword[i]) return 0;
        if (lower((unsigned char)field[i]) != lower((unsigned char)keyword[i])) return 0;
    }
    return 1;
}

static unsigned long value(const char *text, size_t n)
{
    /* WINDOW takes 0x<hex> or decimal, as CON:'s string2val. */
    unsigned long v = 0; size_t i = 0; int hex = 0;
    if (n > 2 && text[0] == '0' && lower((unsigned char)text[1]) == 'x') { hex = 1; i = 2; }
    for (; i < n; ++i) {
        int c = lower((unsigned char)text[i]), d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else break;
        v = v * (hex ? 16 : 10) + (unsigned long)d;
    }
    return v;
}

static int option(XConsoleName *c, const char *f, size_t n)
{
    static const struct { const char *word; unsigned bit; } words[] = {
        {"WAIT", XCON_WAIT}, {"CLOSE", XCON_CLOSE}, {"NOCLOSE", XCON_NOCLOSE},
        {"AUTO", XCON_AUTO}, {"INACTIVE", XCON_INACTIVE}, {"NODEPTH", XCON_NODEPTH},
        {"NOSIZE", XCON_NOSIZE}, {"NODRAG", XCON_NODRAG}, {"NOBORDER", XCON_NOBORDER},
        {"BACKDROP", XCON_BACKDROP}, {"SIMPLE", XCON_SIMPLE}, {"SMART", XCON_SMART},
        {"ALT", XCON_ALT}
    };
    unsigned i;
    for (i = 0; i < sizeof words / sizeof *words; ++i)
        if (prefix(f, n, words[i].word)) {
            c->options |= words[i].bit;
            if (words[i].bit == XCON_CLOSE) c->close_gadget = 1;
            if (words[i].bit == XCON_NOCLOSE) c->close_gadget = 0;
            return 0;
        }
    /* WINDOW and SCREEN compare their six letters and take the rest. */
    if (n >= 6 && prefix(f, 6, "WINDOW")) {
        c->options |= XCON_WINDOW; c->window = value(f + 6, n - 6); return 0;
    }
    if (n >= 6 && prefix(f, 6, "SCREEN")) {
        if (n - 6 >= XCON_SCREEN_MAX) return -1;
        c->options |= XCON_SCREEN;
        memcpy(c->screen, f + 6, n - 6); c->screen[n - 6] = 0;
        return 0;
    }
    if (prefix(f, n, "BOOT")) c->options |= XCON_BOOT;
    return 0;
}

int console_name_parse(XConsoleName *c, const char *name)
{
    const char *colon = strchr(name, ':'), *field;
    unsigned id = 1;
    memset(c, 0, sizeof *c);
    c->left = c->top = c->width = c->height = -1;
    c->close_gadget = -1;
    field = colon ? colon + 1 : name;
    for (;;) {
        const char *end = field;
        size_t n;
        while (*end && *end != '/') ++end;
        n = (size_t)(end - field);
        if (n) {
            if (id <= 4) {
                long v = 0; size_t i;
                for (i = 0; i < n; ++i) {
                    if (field[i] < '0' || field[i] > '9') return -1;
                    if (v < 100000) v = v * 10 + (field[i] - '0');
                }
                if (id == 1) c->left = v;
                else if (id == 2) c->top = v;
                else if (id == 3) c->width = v;
                else c->height = v;
            } else if (id == 5) {
                if (n >= XCON_TITLE_MAX) return -1;
                memcpy(c->title, field, n); c->title[n] = 0;
            } else if (option(c, field, n)) return -1;
        }
        if (!*end) return 0;
        field = end + 1; ++id;
    }
}
