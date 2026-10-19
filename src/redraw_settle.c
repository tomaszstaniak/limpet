#include "redraw_settle.h"

void redraw_settle_init(RedrawSettle *s,int timed,uint32_t quiet_us,uint32_t limit_us)
{
    s->timed=timed; s->held=0; s->first=s->last=0;
    s->quiet=quiet_us; s->limit=limit_us<quiet_us?quiet_us:limit_us;
}

int redraw_settle_output(RedrawSettle *s,uint64_t now)
{
    if(!s->timed) return 1;
    if(!s->held) { s->held=1; s->first=now; }
    s->last=now;
    return 0;
}

static uint64_t due_at(const RedrawSettle *s)
{
    uint64_t quiet=s->last+s->quiet,limit=s->first+s->limit;
    return quiet<limit?quiet:limit;
}

int redraw_settle_due(const RedrawSettle *s,uint64_t now)
{
    return s->held && now>=due_at(s);
}

uint64_t redraw_settle_wake(const RedrawSettle *s,uint64_t now)
{
    uint64_t at;
    if(!s->held) return 0;
    at=due_at(s);
    return now>=at?0:at-now;
}

void redraw_settle_drawn(RedrawSettle *s)
{
    s->held=0; s->first=s->last=0;
}
