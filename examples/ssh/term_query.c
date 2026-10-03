#include "term_query.h"
#include <string.h>

static int xty(const XtTermOps *ops, unsigned operation, XtyControl *c)
{
    memset(c, 0, sizeof *c);
    c->version = XTY_CONTROL_VERSION; c->operation = operation;
    return ops->control(ops->context, c);
}

static int negotiate(XtTermQuery *q, const XtTermOps *ops)
{
    XTermStatus status;
    XtyControl c;
    int rc;
    memset(&status, 0, sizeof status);
    status.version = XTERM_STATUS_VERSION;
    rc = ops->status(ops->context, &status);
    if (rc > 0) { q->mode = XT_TERM_STATUS; return 1; }
    if (rc < 0) return -1;
    rc = xty(ops, XTY_CAPS, &c);
    if (rc < 0) return -1;
    /* Geometry alone cannot drive input without a blocking read; both or
     * neither. */
    if (rc > 0 && (c.values[0] & (XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE)) ==
                  (XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE)) {
        q->mode = XT_TERM_XTY; q->caps = c.values[0]; return 1;
    }
    q->mode = XT_TERM_NONE;
    return 0;
}

int xt_term_query(XtTermQuery *q, const XtTermOps *ops, XtTermSnapshot *s)
{
    memset(s, 0, sizeof *s);
    if (q->mode == XT_TERM_UNKNOWN) {
        int rc = negotiate(q, ops);
        if (rc <= 0) return rc;
    }
    if (q->mode == XT_TERM_STATUS) {
        XTermStatus status;
        memset(&status, 0, sizeof status);
        status.version = XTERM_STATUS_VERSION;
        if (ops->status(ops->context, &status) <= 0) return -1;
        s->columns = status.columns; s->rows = status.rows; s->eof = status.eof;
        s->available = status.available; s->counted = 1; s->data = status.available != 0;
        return 1;
    }
    if (q->mode == XT_TERM_XTY) {
        XtyControl c;
        if (xty(ops, XTY_GET_GEOMETRY, &c) <= 0) return -1;
        s->rows = c.values[0]; s->columns = c.values[1];
        /* A STATE: DATA means at least one byte, never how many. */
        if (xty(ops, XTY_GET_READ_STATE, &c) <= 0) return -1;
        if (c.values[0] != XTY_READ_WAIT && c.values[0] != XTY_READ_DATA &&
            c.values[0] != XTY_READ_EOF) return -1; /* not a waiting state */
        s->data = c.values[0] == XTY_READ_DATA;
        s->eof = c.values[0] == XTY_READ_EOF;
        /* aros_tty_read_try needs both; otherwise read one byte */
        s->try_io = (q->caps & (XTY_CAP_TRY_IO | XTY_CAP_CANCEL)) ==
                    (XTY_CAP_TRY_IO | XTY_CAP_CANCEL);
        return 1;
    }
    return 0;
}
