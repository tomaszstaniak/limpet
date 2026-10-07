#include "input_order.h"
#include "resize_report.h"
#include <string.h>

void input_order_init(XInputOrder *o, XPaste *paste)
{
    memset(o, 0, sizeof *o);
    o->paste = paste;
}

static int framed(const XInputOrder *o)
{
    return o->frame.open || o->frame.in_band || o->frame.offset < o->frame.length;
}

static int resize_half_sent(const XInputOrder *o)
{
    return o->resize.offset && o->resize.offset < o->resize.length;
}

int input_order_paste_busy(const XInputOrder *o) { return framed(o); }

int input_order_others_may_send(const XInputOrder *o)
{
    return !o->frame.in_band && !resize_half_sent(o);
}

int input_order_plain_paste_may_send(const XInputOrder *o)
{
    return !framed(o) && !resize_half_sent(o);
}

void input_order_begin_bracketed(XInputOrder *o)
{
    if (framed(o) || !o->paste->length) return;
    paste_confirm(o->paste);
    paste_frame_begin(&o->frame, 1);
}

static void resize_build(XResizeSlot *r)
{
    r->offset = 0;
    r->length = resize_report((char *)r->bytes, sizeof r->bytes, r->rows, r->columns,
                              r->height, r->width);
}

void input_order_resize(XInputOrder *o, unsigned rows, unsigned columns,
                        unsigned height, unsigned width)
{
    XResizeSlot *r = &o->resize;
    r->rows = rows; r->columns = columns; r->height = height; r->width = width;
    if (resize_half_sent(o)) r->again = 1;
    else resize_build(r);
}

void input_order_cancel_paste(XInputOrder *o)
{
    paste_cancel(o->paste);
    /* Nothing of the frame went out yet: no markers at all. Otherwise the
       pump sends the end marker once the text is gone. */
    if (o->frame.open && !o->frame.in_band) memset(&o->frame, 0, sizeof o->frame);
}

void input_order_drop(XInputOrder *o)
{
    if (framed(o)) paste_cancel(o->paste);
    memset(&o->frame, 0, sizeof o->frame);
    memset(&o->resize, 0, sizeof o->resize);
}

static size_t send(XInputSink sink, void *context, const unsigned char *bytes, size_t n)
{
    size_t taken = n ? sink(context, bytes, n) : 0;
    return taken > n ? n : taken;
}

/* One unit of progress; 0 when nothing is due or the client took nothing. */
static size_t step(XInputOrder *o, int others_idle, XInputSink sink, void *context)
{
    XResizeSlot *r = &o->resize;
    XPasteFrame *f = &o->frame;
    const unsigned char *bytes;
    size_t n, taken;

    if (resize_half_sent(o)) {
        taken = send(sink, context, r->bytes + r->offset, r->length - r->offset);
        r->offset += taken;
        goto resize_done;
    }
    if (f->in_band) {
        n = paste_frame_peek(f, &bytes);
        if (n) { taken = send(sink, context, bytes, n); paste_frame_consume(f, taken); return taken; }
        if (o->paste->length) {
            n = paste_peek(o->paste, &bytes, 1024);
            taken = send(sink, context, bytes, n);
            paste_consume(o->paste, taken);
            return taken;
        }
        if (f->open) {
            paste_frame_end(f);
            n = paste_frame_peek(f, &bytes);
            taken = send(sink, context, bytes, n);
            paste_frame_consume(f, taken);
            return taken;
        }
        return 0;
    }
    if (!others_idle) return 0;
    if (r->length) {
        taken = send(sink, context, r->bytes, r->length);
        r->offset += taken;
        goto resize_done;
    }
    if (f->open && (n = paste_frame_peek(f, &bytes))) {
        taken = send(sink, context, bytes, n);
        paste_frame_consume(f, taken);
        return taken;
    }
    return 0;
resize_done:
    if (r->offset == r->length) {
        r->length = r->offset = 0;
        if (r->again) { r->again = 0; resize_build(r); }
    }
    return taken;
}

size_t input_order_pump(XInputOrder *o, int others_idle, XInputSink sink, void *context)
{
    size_t total = 0, n;
    while ((n = step(o, others_idle, sink, context)) != 0) total += n;
    return total;
}
