#ifndef XTERM_SSH_TERM_QUERY_H
#define XTERM_SSH_TERM_QUERY_H
#include <stdint.h>
#include "../../src/xterm_status.h"
#include "../../src/tty_control.h"

/* What the SSH client may learn about its local terminal, from whichever
 * protocol the input handler answers: the ordinary frontend's
 * XTERM_ACTION_QUERY (byte count), the PTY's XTY_CONTROL (geometry plus a
 * read STATE, never a byte count), or nothing (CON: and other handlers). */
enum { XT_TERM_UNKNOWN=0, XT_TERM_STATUS=1, XT_TERM_XTY=2, XT_TERM_NONE=-1 };

typedef struct {
    /* 1 answered, 0 not known by this handler, -1 any other failure */
    int (*status)(void *context, XTermStatus *);
    int (*control)(void *context, XtyControl *);
    void *context;
} XtTermOps;

typedef struct {
    int mode;
    uint32_t caps; /* XTY capabilities, XT_TERM_XTY only */
} XtTermQuery;

typedef struct {
    unsigned columns, rows; /* zero when unknown */
    int eof;
    int data;           /* at least one byte can be read without waiting */
    int counted;        /* available is a byte count (XT_TERM_STATUS) */
    uint32_t available;
    int try_io;         /* XTY_CAP_TRY_IO: a non-waiting read exists */
} XtTermSnapshot;

/* 1 snapshot filled, 0 nothing to ask (use the fallback), -1 failure after
 * a protocol was negotiated. The first answered protocol is kept. */
int xt_term_query(XtTermQuery *, const XtTermOps *, XtTermSnapshot *);
#endif
