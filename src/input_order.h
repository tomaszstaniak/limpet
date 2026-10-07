#ifndef XTERM_INPUT_ORDER_H
#define XTERM_INPUT_ORDER_H
/* The order in which terminal-made input reaches a raw-mode client: DEC 2048
 * resize reports and DEC 2004 bracketed pastes, against keys and protocol
 * replies.
 *
 * Rules, each of which once failed:
 *   1. Nothing goes inside a bracketed paste. From the first byte of
 *      ESC[200~ to the last byte of ESC[201~ the paste owns the input: keys,
 *      mouse and wheel reports, query replies and resize reports wait. A
 *      resize report between two paste batches arrived inside the paste.
 *   2. Nothing goes inside another report. A resize report that the session
 *      took only part of is finished before anything else is sent.
 *   3. A cancelled bracketed paste still gets its end marker, or the client
 *      reads everything after it as pasted text.
 *   4. A new paste cannot start while the previous one's end marker is still
 *      queued; it used to overwrite it.
 *   5. When the input owner changes (the client leaves raw mode, the session
 *      ends or restarts), a bracketed paste is dropped, text and markers,
 *      without delivery: the rest of it must not reach a cooked Shell
 *      unconfirmed.
 * Portable C, host-tested in tests/test_input_order.c. */
#include "paste.h"
#include <stddef.h>

typedef struct {
    unsigned char bytes[64];
    size_t length, offset;
    int again;                      /* newer size arrived while one was half sent */
    unsigned rows, columns, height, width;
} XResizeSlot;

typedef struct {
    XPaste *paste;
    XPasteFrame frame;
    XResizeSlot resize;
} XInputOrder;

/* Writes up to n bytes to the client and returns how many it took. */
typedef size_t (*XInputSink)(void *context, const unsigned char *bytes, size_t n);

void input_order_init(XInputOrder *, XPaste *);
/* Keys, mouse and wheel reports and query replies may be sent now. */
int input_order_others_may_send(const XInputOrder *);
/* A bracketed paste or its end marker is not finished. */
int input_order_paste_busy(const XInputOrder *);
/* Frame the paste now in o->paste (call after paste_prepare succeeded). */
void input_order_begin_bracketed(XInputOrder *);
/* Queue a report of this size; replaces a queued one, follows a half-sent one. */
void input_order_resize(XInputOrder *, unsigned rows, unsigned columns,
                        unsigned height, unsigned width);
/* Esc and similar: drop the rest of the text, keep the end marker due. */
void input_order_cancel_paste(XInputOrder *);
/* The input owner changed: drop the bracketed paste and the resize report. */
void input_order_drop(XInputOrder *);
/* Send what is due, in order. others_idle: no key, reply or mouse report is
   half delivered. Returns the number of bytes the sink took. Unbracketed
   paste text is not sent here; see input_order_plain_paste_may_send. */
size_t input_order_pump(XInputOrder *, int others_idle, XInputSink, void *);
/* An unbracketed paste may send text now (no report pending or half sent). */
int input_order_plain_paste_may_send(const XInputOrder *);
#endif
