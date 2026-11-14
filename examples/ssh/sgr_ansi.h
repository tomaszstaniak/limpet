#ifndef XTERM_SSH_SGR_ANSI_H
#define XTERM_SSH_SGR_ANSI_H
#include <stddef.h>

/* A terminal that reads SGR 30..37 and 40..47 as Amiga screen pens (Limpet
 * in a console session, as CON: does) shows a remote program's basic colours
 * as pens. This filter rewrites those parameters in the remote output to
 * 38;5;n and 48;5;n, which keep their ANSI meaning there. Everything else
 * passes unchanged, including other control sequences and the arguments of
 * 38, 48 and 58. A sequence split between two calls is held until it ends. */
enum { XT_SGR_HOLD = 96 };

struct xt_sgr_filter {
    int state;
    unsigned char held[XT_SGR_HOLD];
    size_t count;
};

/* The most output one call can produce for `size` input bytes. */
#define XT_SGR_OUTPUT_MAX(size) (4 * (size) + 4 * XT_SGR_HOLD)

void xt_sgr_filter_init(struct xt_sgr_filter *filter);
/* Consumes all of data; returns the number of bytes stored in out, which
 * must hold XT_SGR_OUTPUT_MAX(size). */
size_t xt_sgr_filter(struct xt_sgr_filter *filter, const unsigned char *data,
                     size_t size, unsigned char *out);
#endif
