/* The SSH client's view of its local terminal on the three kinds of input
 * handler: ordinary frontend, PTY, and a handler that knows neither. */
#include "../examples/ssh/term_query.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int knows_status, knows_xty, fail_after;
    unsigned rows, columns, available, eof, read_state, caps;
    unsigned status_calls, control_calls, geometry_calls, state_calls;
} Fake;

static int fake_status(void *context, XTermStatus *s)
{
    Fake *f = context;
    ++f->status_calls;
    if (!f->knows_status) return 0;
    assert(s->version == XTERM_STATUS_VERSION);
    s->columns = f->columns; s->rows = f->rows;
    s->available = f->available; s->eof = f->eof;
    return 1;
}

static int fake_control(void *context, XtyControl *c)
{
    Fake *f = context;
    ++f->control_calls;
    if (!f->knows_xty) return 0;
    if (f->fail_after && f->control_calls > (unsigned)f->fail_after) return -1;
    assert(c->version == XTY_CONTROL_VERSION);
    switch (c->operation) {
    case XTY_CAPS: c->values[0] = f->caps; break;
    case XTY_GET_GEOMETRY:
        ++f->geometry_calls; c->values[0] = f->rows; c->values[1] = f->columns; break;
    case XTY_GET_READ_STATE:
        ++f->state_calls; c->values[0] = f->read_state; break;
    default: assert(!"unexpected operation");
    }
    return 1;
}

static int ask(Fake *f, XtTermQuery *q, XtTermSnapshot *s)
{
    XtTermOps ops = { fake_status, fake_control, f };
    return xt_term_query(q, &ops, s);
}

int main(void)
{
    XtTermQuery q; XtTermSnapshot s; Fake f;

    /* ordinary frontend: the status packet, with a byte count */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_status = 1; f.columns = 100; f.rows = 30; f.available = 7;
    assert(ask(&f, &q, &s) == 1 && q.mode == XT_TERM_STATUS);
    assert(s.columns == 100 && s.rows == 30 && s.counted && s.available == 7 && s.data);
    assert(!f.control_calls);

    /* PTY: no status packet, XTY geometry and read state. The gap was that
     * the client stopped at the status packet and kept 80x24. */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.rows = 18; f.columns = 52; f.read_state = XTY_READ_DATA;
    f.caps = XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE | XTY_CAP_TRY_IO | XTY_CAP_CANCEL;
    assert(ask(&f, &q, &s) == 1 && q.mode == XT_TERM_XTY);
    assert(s.columns == 52 && s.rows == 18);
    /* a read STATE is not a byte count */
    assert(s.data && !s.counted && !s.eof && s.try_io);
    /* a resize is seen on the next query; the protocol is not renegotiated */
    f.rows = 40; f.columns = 120; f.read_state = XTY_READ_WAIT;
    assert(ask(&f, &q, &s) == 1 && s.columns == 120 && s.rows == 40 && !s.data);
    assert(f.status_calls == 1);
    f.read_state = XTY_READ_EOF;
    assert(ask(&f, &q, &s) == 1 && s.eof && !s.data);

    /* PTY without GET_READ_STATE: geometry only is not enough to pump input */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.caps = XTY_CAP_GET_GEOMETRY; f.rows = 18; f.columns = 52;
    assert(ask(&f, &q, &s) == 0 && q.mode == XT_TERM_NONE);

    /* without TRY_IO the client must not attempt a non-waiting read */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.caps = XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE;
    f.rows = 24; f.columns = 80; f.read_state = XTY_READ_DATA;
    assert(ask(&f, &q, &s) == 1 && !s.try_io);

    /* TRY_IO without CANCEL: aros_tty_read_try would refuse, so one byte */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.rows = 24; f.columns = 80; f.read_state = XTY_READ_DATA;
    f.caps = XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE | XTY_CAP_TRY_IO;
    assert(ask(&f, &q, &s) == 1 && s.data && !s.try_io);

    /* an unknown read state is a protocol error, not "no data yet" */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.rows = 24; f.columns = 80; f.read_state = 3;
    f.caps = XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE;
    assert(ask(&f, &q, &s) == -1);

    /* CON: and others: nothing, and nothing asked again */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    assert(ask(&f, &q, &s) == 0 && q.mode == XT_TERM_NONE);
    assert(ask(&f, &q, &s) == 0 && f.status_calls == 1 && f.control_calls == 1);

    /* a failure after negotiation is reported, not hidden as a fallback */
    memset(&q, 0, sizeof q); memset(&f, 0, sizeof f);
    f.knows_xty = 1; f.caps = XTY_CAP_GET_GEOMETRY | XTY_CAP_GET_READ_STATE;
    f.rows = 24; f.columns = 80; f.fail_after = 3;
    assert(ask(&f, &q, &s) == 1);
    assert(ask(&f, &q, &s) == -1);

    puts("SSH TERM QUERY PASS: status, XTY geometry and read state, no byte count, TRY_IO needs CANCEL, bad state, fallback, failure");
    return 0;
}
