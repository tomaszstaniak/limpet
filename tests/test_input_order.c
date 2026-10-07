/* What a raw-mode client reads when a bracketed paste, resize reports,
 * cancellation and owner changes meet. The sink stands for the session's
 * input buffer: it takes at most `room` bytes per call, so every case also
 * runs with partial reads down to one byte. */
#include "../src/input_order.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char got[200000];
static size_t got_length, room = 1u << 30, budget = (size_t)-1;

/* room: most bytes per call (a partial read); budget: bytes until the
   client stops reading for now (queue pressure). */
static size_t sink(void *context, const unsigned char *bytes, size_t n)
{
    size_t take = n < room ? n : room;
    if (take > budget) take = budget;
    budget -= take;
    (void)context;
    assert(got_length + take <= sizeof got);
    memcpy(got + got_length, bytes, take);
    got_length += take;
    return take;
}

static void drain(XInputOrder *o)
{
    int guard = 0;
    budget = (size_t)-1;
    while (input_order_pump(o, 1, sink, NULL)) assert(++guard < 1000000);
}

static int got_is(const char *expected)
{
    return got_length == strlen(expected) && !memcmp(got, expected, got_length);
}

static XPaste paste;
static XInputOrder order;

static void start(const char *text, size_t r)
{
    input_order_init(&order, &paste);
    got_length = 0; room = r;
    assert(!paste_prepare(&paste, (const unsigned char *)text, strlen(text)));
    input_order_begin_bracketed(&order);
}

static void resize_inside_paste(size_t r)
{
    /* A resize while the paste is in band waits for ESC[201~. */
    start("one\ntwo\nthree", r);
    budget = 8;
    assert(input_order_pump(&order, 1, sink, NULL) == 8);  /* marker and "on" */
    assert(!input_order_others_may_send(&order));
    input_order_resize(&order, 30, 100, 480, 800);
    input_order_resize(&order, 31, 101, 496, 808);         /* newest size wins */
    drain(&order);
    assert(got_is("\033[200~one\rtwo\rthree\033[201~\033[48;31;101;496;808t"));
    assert(input_order_others_may_send(&order) && !input_order_paste_busy(&order));
}

static void resize_before_paste(size_t r)
{
    /* A report queued before the paste goes first and is never split. */
    input_order_init(&order, &paste);
    got_length = 0; room = r;
    input_order_resize(&order, 24, 80, 384, 640);
    assert(!paste_prepare(&paste, (const unsigned char *)"x", 1));
    input_order_begin_bracketed(&order);
    drain(&order);
    assert(got_is("\033[48;24;80;384;640t\033[200~x\033[201~"));
}

static void resize_while_report_half_sent(void)
{
    input_order_init(&order, &paste);
    got_length = 0; room = 5; budget = 5;
    input_order_resize(&order, 24, 80, 384, 640);
    assert(input_order_pump(&order, 1, sink, NULL) == 5);
    assert(!input_order_others_may_send(&order));         /* rule 2: finish it first */
    input_order_resize(&order, 10, 40, 160, 320);
    room = 3;
    drain(&order);
    assert(got_is("\033[48;24;80;384;640t\033[48;10;40;160;320t"));
}

static void cancel_in_band(size_t r)
{
    static char big[20000];
    memset(big, 'a', sizeof big - 1);
    start(big, r);
    budget = 100;
    assert(input_order_pump(&order, 1, sink, NULL) == 100);
    input_order_cancel_paste(&order);
    drain(&order);
    assert(got_length >= 12 && !memcmp(got, "\033[200~", 6));
    assert(!memcmp(got + got_length - 6, "\033[201~", 6));  /* rule 3 */
    assert(got_length < 6 + sizeof big);
    assert(!input_order_paste_busy(&order) && !paste.length);
}

static void cancel_before_start(void)
{
    start("abc", 100);
    input_order_cancel_paste(&order);
    drain(&order);
    assert(got_length == 0 && !input_order_paste_busy(&order));
}

static void next_paste_waits(size_t r)
{
    /* Rule 4: the end marker of the first paste is still queued. */
    start("first", r);
    budget = 12;                                /* start marker, text, 1 byte of the end */
    assert(input_order_pump(&order, 1, sink, NULL) == 12);
    assert(input_order_paste_busy(&order) && !paste.length);
    /* The caller refuses a new paste while busy; begin is a no-op as well. */
    assert(!paste_prepare(&paste, (const unsigned char *)"x", 1));
    input_order_begin_bracketed(&order);
    paste_cancel(&paste);
    drain(&order);
    assert(got_is("\033[200~first\033[201~"));
    assert(!input_order_paste_busy(&order));
    assert(got_length >= 12 && !memcmp(got + got_length - 6, "\033[201~", 6));
    assert(!memcmp(got, "\033[200~", 6));
}

static void owner_change(void)
{
    /* Rule 5: client left raw mode during the paste. Nothing more goes out. */
    start("rm -r work\nyes\n", 4);
    budget = 4;
    assert(input_order_pump(&order, 1, sink, NULL) == 4);
    input_order_resize(&order, 24, 80, 384, 640);
    input_order_drop(&order);
    room = 1u << 30;
    drain(&order);
    assert(got_length == 4 && !paste.length);
    assert(!input_order_paste_busy(&order) && input_order_others_may_send(&order));
}

static void others_wait_for_start(void)
{
    /* A key or reply half delivered: the start marker waits for it. */
    start("x", 100);
    assert(input_order_pump(&order, 0, sink, NULL) == 0 && got_length == 0);
    assert(input_order_others_may_send(&order));
    drain(&order);
    assert(got_is("\033[200~x\033[201~"));
}

static void two_pastes_in_a_row(void)
{
    start("a\nb", 3);
    drain(&order);
    assert(!paste_prepare(&paste, (const unsigned char *)"c", 1));
    input_order_begin_bracketed(&order);
    drain(&order);
    assert(got_is("\033[200~a\rb\033[201~\033[200~c\033[201~"));
}

int main(void)
{
    size_t r;
    static const size_t rooms[] = { 1, 2, 3, 5, 7, 64, 1u << 30 };
    for (r = 0; r < sizeof rooms / sizeof rooms[0]; ++r) {
        resize_inside_paste(rooms[r]);
        resize_before_paste(rooms[r]);
        cancel_in_band(rooms[r]);
        next_paste_waits(rooms[r]);
    }
    resize_while_report_half_sent();
    cancel_before_start();
    owner_change();
    others_wait_for_start();
    two_pastes_in_a_row();
    puts("input order tests passed: paste framing, resize, cancel, owner change, partial reads");
    return 0;
}
