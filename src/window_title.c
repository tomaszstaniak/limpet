#include "window_title.h"
#include <stdio.h>
#include <string.h>

typedef struct { char *out; size_t size, used; } Sink;

static void put_byte(Sink *s, unsigned char c)
{
    if (s->used + 1 < s->size) s->out[s->used++] = (char)c;
}

static void put_ascii(Sink *s, const char *text)
{
    while (*text) put_byte(s, (unsigned char)*text++);
}

/* System-charset bytes: controls (C0, DEL, C1) would draw as boxes or worse. */
static void put_native(Sink *s, const char *text, size_t limit)
{
    size_t i;
    for (i = 0; text[i] && i < limit; ++i) {
        unsigned char c = (unsigned char)text[i];
        put_byte(s, c < 0x20 || (c >= 0x7f && c <= 0x9f) ? '?' : c);
    }
}

/* UTF-8 already validated by the screen model; still defensive: a bad byte
 * becomes '?' and decoding continues with the next one. */
static void put_utf8(Sink *s, const char *text)
{
    const unsigned char *t = (const unsigned char *)text;
    while (*t) {
        unsigned long c;
        size_t len, k;
        if (*t < 0x80) { put_byte(s, *t >= 0x20 && *t != 0x7f ? *t : '?'); ++t; continue; }
        if (*t >= 0xc2 && *t <= 0xdf) { len = 2; c = *t & 0x1f; }
        else if (*t >= 0xe0 && *t <= 0xef) { len = 3; c = *t & 0x0f; }
        else if (*t >= 0xf0 && *t <= 0xf4) { len = 4; c = *t & 0x07; }
        else { put_byte(s, '?'); ++t; continue; }
        for (k = 1; k < len; ++k) {
            if ((t[k] & 0xc0) != 0x80) break;
            c = (c << 6) | (t[k] & 0x3f);
        }
        if (k < len) { put_byte(s, '?'); ++t; continue; }
        put_byte(s, c >= 0xa0 && c <= 0xff ? (unsigned char)c : '?');
        t += len;
    }
}

const char *window_title_command_name(const char *name)
{
    const char *part = name;
    if (!name) return NULL;
    for (; *name; ++name)
        if (*name == ':' || *name == '/') part = name + 1;
    return part;
}

size_t window_title_compose(char *out, size_t size, const XWindowTitle *in)
{
    Sink s;
    const char *command;
    char number[48];
    if (!out || !size) return 0;
    s.out = out; s.size = size; s.used = 0;
    if (!in) { out[0] = 0; return 0; }
    command = in->command ? window_title_command_name(in->command) : NULL;
    if (in->link) {
        /* Long URLs: the start says where it goes. */
        put_ascii(&s, "Link: ");
        put_native(&s, in->link, 100);
        if (strlen(in->link) > 100) put_ascii(&s, "...");
        put_ascii(&s, " - ");
    } else if (in->program && in->program[0]) {
        put_utf8(&s, in->program);
        put_ascii(&s, " - ");
    } else if (command && command[0]) {
        put_native(&s, command, 64);
        put_ascii(&s, " - ");
    } else if (in->shell) {
        put_ascii(&s, "Shell - ");
    }
    put_ascii(&s, in->terminal ? in->terminal : "Limpet");
    if (in->status && in->status[0]) {
        put_ascii(&s, " | ");
        put_native(&s, in->status, (size_t)-1);
    }
    if (in->view_offset) {
        snprintf(number, sizeof number, " | scrollback %u/%u", in->view_offset, in->history_count);
        put_ascii(&s, number);
    }
    out[s.used] = 0;
    return s.used;
}

static int same_owner(const XProgramTitle *t, const XForeground *f)
{
    if (!t->owner_known) return 1;
    if (!f || !f->known) return 1;      /* unreadable now: no evidence of a change */
    return f->running == t->owner_running && (!f->running || f->module == t->owner_module);
}

void program_title_offer(XProgramTitle *t, const char *title, unsigned long serial,
                         const XForeground *f)
{
    if (!t || serial == t->serial) return;
    t->serial = serial;
    if (!title || !title[0]) { t->set = 0; t->text[0] = 0; return; }
    snprintf(t->text, sizeof t->text, "%s", title);
    t->set = 1;
    t->owner_known = f && f->known;
    t->owner_running = t->owner_known && f->running;
    t->owner_module = t->owner_known ? f->module : 0;
}

int program_title_follow(XProgramTitle *t, const XForeground *f)
{
    if (!t || !t->set || same_owner(t, f)) return 0;
    t->set = 0; t->text[0] = 0;
    return 1;
}

const char *program_title_text(const XProgramTitle *t)
{ return t && t->set ? t->text : NULL; }

void program_title_clear(XProgramTitle *t)
{
    if (!t) return;
    t->set = 0; t->text[0] = 0;
}
