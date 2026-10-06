#ifndef XTERM_RESIZE_REPORT_H
#define XTERM_RESIZE_REPORT_H
#include <stdio.h>
#include <stddef.h>
/* DEC mode 2048 in-band resize report, as other terminals send it:
 * CSI 48 ; rows ; columns ; height px ; width px t. Writes nothing and
 * returns 0 when it does not fit. */
static inline size_t resize_report(char *out, size_t cap, unsigned rows, unsigned cols,
                                   unsigned hpx, unsigned wpx)
{
    char buf[64];
    int n = snprintf(buf, sizeof buf, "\033[48;%u;%u;%u;%ut", rows, cols, hpx, wpx);
    size_t i;
    if (n <= 0 || (size_t)n >= cap || (size_t)n >= sizeof buf) return 0;
    for (i = 0; i <= (size_t)n; ++i) out[i] = buf[i];
    return (size_t)n;
}
#endif
