#include "sgr_ansi.h"

#include <string.h>

enum { TEXT, ESCAPE, PARAMETERS, PASS };

void xt_sgr_filter_init(struct xt_sgr_filter *filter)
{
    memset(filter, 0, sizeof *filter);
}

static size_t put_number(unsigned char *out, unsigned value)
{
    unsigned char digits[10];
    size_t n = 0, i;
    do { digits[n++] = (unsigned char)('0' + value % 10); value /= 10; } while (value);
    for (i = 0; i < n; ++i) out[i] = digits[n - 1 - i];
    return n;
}

/* held is "ESC [ parameters" of an SGR sequence, digits and ';' only. */
static size_t rewrite(const unsigned char *held, size_t count, unsigned char *out)
{
    size_t at = 2, n = 2, start, i;
    int skip = 0; /* parameters still to copy as arguments of 38/48/58 */
    int extended = 0; /* the next parameter selects the form of 38/48/58 */
    out[0] = 0x1b; out[1] = '[';
    while (at <= count) {
        unsigned value = 0;
        int empty;
        start = at;
        while (at < count && held[at] != ';') {
            if (value < 100000) value = value * 10 + (unsigned)(held[at] - '0');
            ++at;
        }
        empty = at == start;
        if (extended) {
            extended = 0;
            skip = value == 5 ? 1 : value == 2 ? 3 : 0;
            for (i = start; i < at; ++i) out[n++] = held[i];
        } else if (skip) {
            --skip;
            for (i = start; i < at; ++i) out[n++] = held[i];
        } else if (!empty && ((value >= 30 && value <= 37) || (value >= 40 && value <= 47))) {
            out[n++] = value < 40 ? '3' : '4';
            out[n++] = '8'; out[n++] = ';'; out[n++] = '5'; out[n++] = ';';
            n += put_number(out + n, value % 10);
        } else {
            if (!empty && (value == 38 || value == 48 || value == 58)) extended = 1;
            for (i = start; i < at; ++i) out[n++] = held[i];
        }
        if (at < count) out[n++] = ';';
        ++at;
    }
    out[n++] = 'm';
    return n;
}

static size_t release(struct xt_sgr_filter *f, unsigned char *out)
{
    size_t n = f->count;
    memcpy(out, f->held, n);
    f->count = 0;
    return n;
}

size_t xt_sgr_filter(struct xt_sgr_filter *f, const unsigned char *data,
                     size_t size, unsigned char *out)
{
    size_t n = 0, i;
    for (i = 0; i < size; ++i) {
        unsigned char c = data[i];
        switch (f->state) {
        case TEXT:
            if (c == 0x1b) { f->held[0] = c; f->count = 1; f->state = ESCAPE; }
            else out[n++] = c;
            break;
        case ESCAPE:
            if (c == '[') { f->held[f->count++] = c; f->state = PARAMETERS; break; }
            n += release(f, out + n);
            if (c == 0x1b) { f->held[0] = c; f->count = 1; }
            else { out[n++] = c; f->state = TEXT; }
            break;
        case PARAMETERS:
            if (((c >= '0' && c <= '9') || c == ';') && f->count < XT_SGR_HOLD) {
                f->held[f->count++] = c;
            } else if (c == 'm') {
                n += rewrite(f->held, f->count, out + n);
                f->count = 0; f->state = TEXT;
            } else {
                /* Not a plain SGR sequence, or too long to hold: unchanged. */
                n += release(f, out + n);
                if (c == 0x1b) { f->held[0] = c; f->count = 1; f->state = ESCAPE; }
                else { out[n++] = c; f->state = c >= 0x40 && c <= 0x7e ? TEXT : PASS; }
            }
            break;
        default: /* PASS: the rest of a control sequence this filter leaves alone */
            if (c == 0x1b) { f->held[0] = c; f->count = 1; f->state = ESCAPE; }
            else { out[n++] = c; if (c >= 0x40 && c <= 0x7e) f->state = TEXT; }
            break;
        }
    }
    return n;
}
