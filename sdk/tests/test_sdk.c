/* Host tests of the Limpet SDK contracts. The "handler" behind DoPkt is
   either Limpet's own reply code (src/xterm_presentation.h, the code the
   terminal ships) or a scripted misbehaviour. This proves the client and
   the server agree on the wire; it does not prove anything about AROS. */
#include <limpet/limpet.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include "../../src/xterm_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LIMPET_REPLY, CON_REPLY, SILENT_REFUSAL, ECHO_BLOCK, BAD_RESULT,
       SUCCESS_WITH_ERROR, FUTURE_REPLY, ZERO_GEOMETRY, OTHER_ERROR };
#define CONSOLE_PTR 1
static int mode, packets, interactive = 1;
static unsigned rows = 24, columns = 80, pens = XTY_COLOR_PENS;
static LONG last_error;
static SIPTR last_action, last_arg3, last_arg4, last_arg5;

LONG IoErr(void) { return last_error; }
LONG SetIoErr(LONG code) { LONG old = last_error; last_error = code; return old; }
LONG IsInteractive(BPTR handle) { (void)handle; return interactive ? DOSTRUE : DOSFALSE; }

SIPTR DoPkt(void *port, LONG action, SIPTR a1, SIPTR a2, SIPTR a3, SIPTR a4, SIPTR a5)
{
    XtyControl *block = (XtyControl *)a2;
    (void)port; (void)a1;
    ++packets;
    last_action = action; last_arg3 = a3; last_arg4 = a4; last_arg5 = a5;
    switch (mode) {
    case LIMPET_REPLY:
        if (!xterm_presentation_reply(block, (size_t)a3, rows, columns, pens)) {
            last_error = ERROR_BAD_NUMBER; return DOSFALSE;
        }
        last_error = 0; return DOSTRUE;
    case CON_REPLY:
        memset(block, 0x55, sizeof *block); /* a handler may scribble */
        last_error = ERROR_ACTION_NOT_KNOWN; return DOSFALSE;
    case SILENT_REFUSAL: last_error = 0; return DOSFALSE;
    case ECHO_BLOCK: last_error = 0; return DOSTRUE;
    case BAD_RESULT: last_error = 0; return 0xffffffffLL;
    case SUCCESS_WITH_ERROR:
        xterm_presentation_reply(block, (size_t)a3, rows, columns, pens);
        last_error = ERROR_OBJECT_IN_USE; return DOSTRUE;
    case FUTURE_REPLY:
        xterm_presentation_reply(block, (size_t)a3, rows, columns, pens);
        block->values[0] |= 0x80000000u | 64u;  /* known bits from the future */
        block->values[2] |= 0x40u; block->values[6] |= 0x100u;
        block->values[7] = 7; block->values[11] = 11;
        last_error = 0; return DOSTRUE;
    case ZERO_GEOMETRY:
        xterm_presentation_reply(block, (size_t)a3, rows, columns, pens);
        block->values[0] |= XTY_PRESENT_KNOWN_GEOMETRY; block->values[4] = 0;
        last_error = 0; return DOSTRUE;
    default: last_error = 303; return DOSFALSE;
    }
}

/* Write() that accepts at most `write_limit` bytes per call, then fails
   after `write_budget` calls: checks that limpet_write_dos resends only
   the unwritten suffix. */
static char dos_out[256];
static size_t dos_length;
static LONG write_limit = 3;
static int write_budget = 1000;
LONG Write(BPTR handle, APTR buffer, LONG length)
{
    LONG n = length < write_limit ? length : write_limit;
    (void)handle;
    if (write_budget-- <= 0) return -1;
    memcpy(dos_out + dos_length, buffer, (size_t)n);
    dos_length += (size_t)n;
    return n;
}

static void test_write_dos(void)
{
    const char *text = "\033[38;5;2mgreen\033[39m";
    dos_length = 0;
    assert(limpet_write_dos((void *)CONSOLE_PTR, text, strlen(text)) == 0);
    assert(dos_length == strlen(text) && !memcmp(dos_out, text, dos_length));
    dos_length = 0; write_budget = 2;
    assert(limpet_write_dos((void *)CONSOLE_PTR, text, strlen(text)) == 1 && dos_length == 6);
    write_budget = 1000;
}

static unsigned char written[1 << 16];
static size_t written_length;
static int fail_after = -1, write_calls, write_attempts;
static char last_attempt[16];
static int collect(void *context, const char *bytes, size_t length)
{
    (void)context;
    ++write_attempts;
    if (length < sizeof last_attempt) { memcpy(last_attempt, bytes, length); last_attempt[length] = 0; }
    if (fail_after >= 0 && write_calls >= fail_after) return 7;
    ++write_calls;
    assert(written_length + length <= sizeof written);
    memcpy(written + written_length, bytes, length);
    written_length += length;
    return 0;
}

static size_t counted_bytes, counted_calls;
static int count_only(void *context, const char *bytes, size_t length)
{
    (void)context;
    assert(length <= LIMPET_KITTY_CHUNK + 96 && bytes[0] == '\033' && bytes[length - 1] == '\\');
    counted_bytes += length; ++counted_calls;
    return 0;
}

static struct FileHandle console = { (void *)&console, 1 }, nil = { 0, 0 };
#define CONSOLE ((BPTR)&console)

static LimpetPresentation query(LimpetStatus expect, LONG expect_error)
{
    LimpetPresentation p; LONG error = -5;
    memset(&p, 0xaa, sizeof p);
    assert(limpet_query_presentation(CONSOLE, &p, &error) == expect);
    assert(error == expect_error);
    if (expect != LIMPET_OK) {
        LimpetPresentation zero; memset(&zero, 0, sizeof zero);
        assert(!memcmp(&p, &zero, sizeof p));
    }
    return p;
}

static void test_query(void)
{
    LimpetPresentation p; XtyControl c, before;

    mode = LIMPET_REPLY;
    p = query(LIMPET_OK, 0);
    assert(last_action == XTY_ACTION_CONTROL && last_arg3 == 64 &&
           last_arg4 == XTY_PACKET_MAGIC && last_arg5 == 0);
    assert(p.known == 63 && limpet_color_level(&p) == XTY_COLOR_RGB);
    assert(p.input == (XTY_INPUT_BRACKETED_PASTE | XTY_INPUT_RESIZE_REPORT));
    assert(limpet_uses_pens(&p) && limpet_has_bold(&p) && limpet_can_rewrite_line(&p));
    assert(limpet_rows(&p) == 24 && limpet_columns(&p) == 80);

    /* Geometry is a snapshot: a later resize shows only in a new query. */
    rows = 18; columns = 63;
    assert(limpet_columns(&p) == 80);
    p = query(LIMPET_OK, 0);
    assert(limpet_rows(&p) == 18 && limpet_columns(&p) == 63);

    /* Unknown geometry (a window not yet sized) is not an error. */
    rows = 0; p = query(LIMPET_OK, 0);
    assert(!(p.known & XTY_PRESENT_KNOWN_GEOMETRY) && !limpet_columns(&p) && !p.rows);
    rows = 24; columns = 80;

    pens = 0; p = query(LIMPET_OK, 0);
    assert(!limpet_uses_pens(&p) && (p.known & XTY_PRESENT_KNOWN_INTERPRETATION));
    pens = XTY_COLOR_PENS;

    mode = CON_REPLY; query(LIMPET_UNSUPPORTED, ERROR_ACTION_NOT_KNOWN);
    mode = SILENT_REFUSAL; query(LIMPET_UNSUPPORTED, ERROR_ACTION_NOT_KNOWN);
    mode = ECHO_BLOCK; query(LIMPET_UNSUPPORTED, ERROR_ACTION_NOT_KNOWN);
    mode = BAD_RESULT; query(LIMPET_INVALID_REPLY, ERROR_BAD_NUMBER);
    mode = SUCCESS_WITH_ERROR; query(LIMPET_INVALID_REPLY, ERROR_BAD_NUMBER);
    mode = ZERO_GEOMETRY; query(LIMPET_INVALID_REPLY, ERROR_BAD_NUMBER);
    mode = OTHER_ERROR; query(LIMPET_FAILED, 303);

    /* Newer terminal: unknown bits and reserved values are kept or ignored,
       never a reason to fall back. */
    mode = FUTURE_REPLY; p = query(LIMPET_OK, 0);
    assert(p.known & 0x80000000u);
    assert(limpet_has_bold(&p) && limpet_uses_pens(&p) && limpet_columns(&p) == 80);
    assert(p.styles & 0x40u && p.interpretation & 0x100u);

    /* Not interactive: no packet at all. */
    mode = LIMPET_REPLY; interactive = 0; packets = 0;
    query(LIMPET_NOT_INTERACTIVE, 0);
    assert(packets == 0);
    interactive = 1;

    /* limpet_control leaves the caller's block alone on failure. */
    mode = CON_REPLY; limpet_presentation_request(&c); before = c;
    assert(limpet_control(CONSOLE, &c) == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN);
    assert(!memcmp(&c, &before, sizeof c));
    assert(limpet_control((BPTR)&nil, &c) == DOSFALSE && IoErr() == ERROR_INVALID_LOCK);
    assert(limpet_control(0, &c) == DOSFALSE && IoErr() == ERROR_BAD_NUMBER);
    assert(!memcmp(&c, &before, sizeof c));

    /* A request with a stray value is rejected by Limpet and reported. */
    mode = LIMPET_REPLY; c.values[3] = 1; before = c;
    assert(limpet_control(CONSOLE, &c) == DOSFALSE && IoErr() == ERROR_BAD_NUMBER);
    assert(!memcmp(&c, &before, sizeof c));

    assert(limpet_query_presentation(CONSOLE, NULL, NULL) == LIMPET_BAD_ARGUMENT);
    assert(limpet_query_presentation(0, &p, NULL) == LIMPET_BAD_ARGUMENT);
}

static void test_decode(void)
{
    XtyControl c; LimpetPresentation p; unsigned i;
    limpet_presentation_request(&c);
    for (i = 0; i < 12; ++i) assert(!c.values[i]);
    assert(c.version == 1 && c.operation == 13 && !c.reserved[0] && !c.reserved[1]);
    /* An unanswered request is not a reply. */
    assert(limpet_presentation_decode(&c, &p) == LIMPET_INVALID_REPLY);
    assert(xterm_presentation_reply(&c, sizeof c, 30, 100, 0));
    assert(limpet_presentation_decode(&c, &p) == LIMPET_OK && p.columns == 100);
    /* A reply from Limpet 0.1.0 has no input bit: input stays 0, even if
       values[7] were not zero. */
    {
        XtyControl old = c;
        old.values[0] &= ~XTY_PRESENT_KNOWN_INPUT;
        old.values[7] = 5;
        assert(limpet_presentation_decode(&old, &p) == LIMPET_OK && !p.input);
        assert(!(p.known & XTY_PRESENT_KNOWN_INPUT) && p.columns == 100);
    }
    c.operation = XTY_GET_GEOMETRY;
    assert(limpet_presentation_decode(&c, &p) == LIMPET_INVALID_REPLY && !p.known);
    /* Fields behind a clear known bit are zeroed, whatever the wire says. */
    limpet_presentation_request(&c);
    assert(xterm_presentation_reply(&c, sizeof c, 30, 100, XTY_COLOR_PENS));
    c.values[0] = XTY_PRESENT_KNOWN_LINES;
    assert(limpet_presentation_decode(&c, &p) == LIMPET_OK);
    assert(!p.color_level && !p.styles && !p.rows && !p.columns && !p.interpretation && !p.input);
    assert(!limpet_uses_pens(&p) && limpet_color_level(&p) == XTY_COLOR_NONE);
    assert(limpet_can_rewrite_line(&p));
    /* PENS without its known bit is no claim. */
    p.interpretation = XTY_COLOR_PENS;
    assert(!limpet_uses_pens(&p));
    /* CR alone is not enough for an in-place line. */
    p.lines = XTY_LINE_CR;
    assert(!limpet_can_rewrite_line(&p));
    /* A colour level from the future is treated as unknown. */
    p.known = XTY_PRESENT_KNOWN_COLORS; p.color_level = 9;
    assert(limpet_color_level(&p) == XTY_COLOR_NONE);
    assert(!limpet_uses_pens(NULL) && !limpet_columns(NULL) && !limpet_color_level(NULL));
}

static LimpetPresentation level(uint32_t colors, int with_pens)
{
    LimpetPresentation p; memset(&p, 0, sizeof p);
    p.known = XTY_PRESENT_KNOWN_COLORS | XTY_PRESENT_KNOWN_INTERPRETATION;
    p.color_level = colors; p.interpretation = with_pens ? XTY_COLOR_PENS : 0;
    return p;
}

static void test_sgr(void)
{
    char b[LIMPET_SEQUENCE_MAX];
    LimpetPresentation ansi = level(XTY_COLOR_ANSI16, 0), pen = level(XTY_COLOR_ANSI16, 1);
    LimpetPresentation none = level(XTY_COLOR_NONE, 0), cube = level(XTY_COLOR_INDEXED256, 1);
    LimpetPresentation rgb = level(XTY_COLOR_RGB, 1), unknown;
    memset(&unknown, 0, sizeof unknown);

    assert(limpet_sgr_ansi(b, sizeof b, &ansi, 1, 0) == 5 && !strcmp(b, "\033[31m"));
    assert(limpet_sgr_ansi(b, sizeof b, &ansi, 9, 0) && !strcmp(b, "\033[91m"));
    assert(limpet_sgr_ansi(b, sizeof b, &ansi, 4, 1) && !strcmp(b, "\033[44m"));
    assert(limpet_sgr_ansi(b, sizeof b, &ansi, 15, 1) && !strcmp(b, "\033[107m"));
    assert(limpet_sgr_ansi(b, sizeof b, &pen, 2, 0) && !strcmp(b, "\033[38;5;2m"));
    assert(limpet_sgr_ansi(b, sizeof b, &pen, 12, 1) && !strcmp(b, "\033[48;5;12m"));
    assert(!limpet_sgr_ansi(b, sizeof b, &none, 1, 0) && !b[0]);
    assert(!limpet_sgr_ansi(b, sizeof b, &unknown, 1, 0) && !b[0]);
    assert(!limpet_sgr_ansi(b, sizeof b, &ansi, 16, 0) && !b[0]);
    assert(!limpet_sgr_ansi(b, 5, &ansi, 1, 0) && !b[0]);   /* no truncated escape */
    assert(limpet_sgr_ansi(b, 6, &ansi, 1, 0) == 5);

    assert(limpet_sgr_rgb(b, sizeof b, &rgb, 255, 128, 0, 0) && !strcmp(b, "\033[38;2;255;128;0m"));
    assert(limpet_sgr_rgb(b, sizeof b, &cube, 255, 0, 0, 0) && !strcmp(b, "\033[38;5;196m"));
    assert(limpet_sgr_rgb(b, sizeof b, &cube, 0, 0, 0, 1) && !strcmp(b, "\033[48;5;16m"));
    assert(limpet_sgr_rgb(b, sizeof b, &cube, 255, 255, 255, 0) && !strcmp(b, "\033[38;5;231m"));
    assert(limpet_sgr_rgb(b, sizeof b, &cube, 95, 135, 175, 0) && !strcmp(b, "\033[38;5;67m"));
    assert(!limpet_sgr_rgb(b, sizeof b, &pen, 1, 2, 3, 0) && !b[0]);
    assert(!limpet_sgr_rgb(b, sizeof b, &rgb, 256, 0, 0, 0));
    assert(!limpet_sgr_rgb(b, 8, &rgb, 1, 2, 3, 0) && !b[0]);
}

static void test_osc8(void)
{
    char b[LIMPET_OSC8_URI_MAX + 16], uri[LIMPET_OSC8_URI_MAX + 2];
    assert(limpet_osc8_open(b, sizeof b, "https://aros.org/") == 24);
    assert(!strcmp(b, "\033]8;;https://aros.org/\033\\"));
    assert(!limpet_osc8_open(b, sizeof b, "https://a.org/\033]0;x") && !b[0]);
    assert(!limpet_osc8_open(b, sizeof b, "https://a.org/a b") && !b[0]);
    assert(!limpet_osc8_open(b, sizeof b, "https://a.org/\x07"));
    assert(!limpet_osc8_open(b, sizeof b, "https://zażółć.pl"));
    assert(!limpet_osc8_open(b, sizeof b, ""));
    assert(!limpet_osc8_open(b, sizeof b, NULL));
    memset(uri, 'a', LIMPET_OSC8_URI_MAX); uri[LIMPET_OSC8_URI_MAX] = 0;
    assert(limpet_osc8_open(b, sizeof b, uri) == LIMPET_OSC8_URI_MAX + 7);
    uri[LIMPET_OSC8_URI_MAX] = 'a'; uri[LIMPET_OSC8_URI_MAX + 1] = 0;
    assert(!limpet_osc8_open(b, sizeof b, uri));
    assert(!limpet_osc8_open(b, 10, "https://aros.org/") && !b[0]);
    assert(!strcmp(LIMPET_OSC8_CLOSE, "\033]8;;\033\\"));
}

/* Decode the base64 payloads of every sequence in `written` back into
   bytes, checking framing and m= flags on the way. */
static size_t unpack(unsigned char *out, size_t cap, int *sequences, char *first_keys, size_t keys_cap)
{
    size_t at = 0, n = 0; int last_more = 1;
    *sequences = 0;
    while (at < written_length) {
        size_t semi, end, i; unsigned v = 0, bits = 0;
        assert(last_more);
        assert(!memcmp(written + at, "\033_G", 3));
        for (semi = at; written[semi] != ';'; ++semi) {}
        for (end = semi; !(written[end] == '\033' && written[end + 1] == '\\'); ++end) {}
        if (!*sequences) {
            assert(semi - at - 3 < keys_cap);
            memcpy(first_keys, written + at + 3, semi - at - 3); first_keys[semi - at - 3] = 0;
        }
        assert(end - semi - 1 <= LIMPET_KITTY_CHUNK && (end - semi - 1) % 4 == 0);
        last_more = written[semi - 1] == '1';
        for (i = semi + 1; i < end; ++i) {
            const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            if (written[i] == '=') { assert(end - i <= 2 && !last_more); continue; }
            v = v << 6 | (unsigned)(strchr(t, written[i]) - t); bits += 6;
            if (bits >= 8) { bits -= 8; assert(n < cap); out[n++] = (unsigned char)(v >> bits); }
        }
        ++*sequences; at = end + 2;
    }
    assert(!last_more);
    return n;
}

static void test_kitty(void)
{
    static unsigned char pixels[1024 * 1024 * 4], back[1024 * 1024 * 4];
    LimpetImage image = { 7, 3, 2, 24, 1, 2 };
    char keys[128], b[96], msg[40]; int sequences; size_t i, n;

    for (i = 0; i < sizeof pixels; ++i) pixels[i] = (unsigned char)(i * 7 + 3);
    written_length = 0;
    assert(limpet_kitty_transmit(&image, pixels, collect, NULL) == 0);
    n = unpack(back, sizeof back, &sequences, keys, sizeof keys);
    assert(sequences == 1 && n == 18 && !memcmp(back, pixels, 18));
    assert(!strcmp(keys, "a=T,f=24,t=d,s=3,v=2,i=7,C=1,q=2,m=0"));

    /* Sizes around and across chunk boundaries (3072 raw bytes each). */
    for (i = 1; i <= 1024; i += 13) {
        unsigned h;
        for (h = 1; h <= 3; ++h) {
            LimpetImage w = { 9, (unsigned)i, h, 24, 0, 0 };
            written_length = 0;
            assert(limpet_kitty_transmit(&w, pixels, collect, NULL) == 0);
            n = unpack(back, sizeof back, &sequences, keys, sizeof keys);
            assert(n == i * h * 3 && !memcmp(back, pixels, n));
            assert(sequences == (int)((n + 3071) / 3072));
        }
    }

    /* The largest image Limpet accepts: counted, not stored. */
    image.width = image.height = 1024; image.format = 32;
    counted_bytes = counted_calls = 0;
    assert(limpet_kitty_transmit(&image, pixels, count_only, NULL) == 0);
    assert(counted_calls == (4u << 20) / 3072 + 1);
    /* One pixel more is refused before anything is written. */
    image.height = 1025; counted_calls = 0;
    assert(limpet_kitty_transmit(&image, pixels, count_only, NULL) == -1 && !counted_calls);
    image.height = 2; image.width = 3; image.format = 24;
    image.id = 0; assert(limpet_kitty_transmit(&image, pixels, collect, NULL) == -1);
    image.id = 7; image.format = 16; assert(limpet_kitty_transmit(&image, pixels, collect, NULL) == -1);
    image.format = 24; image.quiet = 3; assert(limpet_kitty_transmit(&image, pixels, collect, NULL) == -1);
    image.quiet = 2; assert(limpet_kitty_transmit(&image, NULL, collect, NULL) == -1);
    assert(limpet_kitty_transmit(&image, pixels, NULL, NULL) == -1);

    /* A failing writer stops the transmission and its code comes back. */
    {
        LimpetImage w = { 9, 1024, 3, 24, 0, 2 };
        written_length = 0; write_calls = 0; fail_after = 1;
        write_attempts = 0;
        assert(limpet_kitty_transmit(&w, pixels, collect, NULL) == 7 && write_calls == 1);
        /* the open transmission is ended with an empty final chunk */
        assert(write_attempts == 3 && !strcmp(last_attempt, "\033_Gm=0;\033\\"));
        /* a failure on the first chunk leaves nothing open: no extra write */
        write_attempts = 0; write_calls = 0; fail_after = 0;
        assert(limpet_kitty_transmit(&w, pixels, collect, NULL) == 7 && write_attempts == 1);
        fail_after = -1;
    }

    written_length = 0;
    assert(!limpet_kitty_delete(7, 1, collect, NULL));
    assert(written_length == strlen("\033_Ga=d,d=I,i=7,q=2\033\\") &&
           !memcmp(written, "\033_Ga=d,d=I,i=7,q=2\033\\", written_length));
    written_length = 0;
    assert(!limpet_kitty_delete(0, 0, collect, NULL));
    assert(!memcmp(written, "\033_Ga=d,d=a,q=2\033\\", written_length));

    assert(limpet_kitty_query(b, sizeof b, 31) && !strcmp(b, "\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\"));
    assert(!limpet_kitty_query(b, sizeof b, 0) && !b[0]);
    assert(!limpet_kitty_query(b, 10, 31) && !b[0]);

    assert(limpet_kitty_parse_reply("\033_Gi=31;OK\033\\", 12, 31, msg, sizeof msg) == 1);
    assert(limpet_kitty_parse_reply("x\033_Gi=31;OK\033\\", 13, 31, NULL, 0) == 1);
    assert(limpet_kitty_parse_reply("\033_Gi=31;OK\033\\", 12, 3, msg, sizeof msg) == -1);
    assert(limpet_kitty_parse_reply("\033_Gi=31;OK\033", 11, 31, msg, sizeof msg) == -1);
    assert(limpet_kitty_parse_reply("\033_Gi=31;ENOTSUPPORTED:no\033\\", 27, 31, msg, sizeof msg) == 0);
    assert(!strcmp(msg, "ENOTSUPPORTED:no"));
    assert(limpet_kitty_parse_reply("\033_Gi=31;EINVAL:long text here\033\\", 31, 31, msg, 7) == 0);
    assert(!strcmp(msg, "EINVAL"));
    assert(limpet_kitty_parse_reply("", 0, 31, msg, sizeof msg) == -1);
}

int main(void)
{
    test_decode();
    test_query();
    test_sgr();
    test_write_dos();
    test_osc8();
    test_kitty();
    assert(!strcmp(LIMPET_SDK_VERSION_STRING, "0.1.0"));
    puts("LIMPET SDK PASS: query against the terminal's reply code, refusals, malformed and future replies, "
         "SGR, OSC 8, Kitty framing and limits");
    return 0;
}
