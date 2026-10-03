#include "aros_tty_signal.h"
static void deliver_interrupt(void *context)
{
    ArosTtySignal *r=context;
    if(aros_break_send(&r->gate)) ++r->delivered;
    else ++r->dropped; /* A removed foreground task is not a new task target. */
}
TtyStatus aros_tty_signal_open(ArosTtySignal *r,TtyService *s,struct Task *task)
{
    if(!r || !s || !task) return TTY_INVALID;
    if(r->service || s->interrupt || s->pair.interrupt_pending) return TTY_BUSY;
    if(s->signal_generation==UINT64_MAX) return TTY_BUSY;
    if(!s->pair.refs[TTY_MASTER] || !s->pair.refs[TTY_SLAVE]) return TTY_CLOSED;
    aros_break_open(&r->gate);
    if(!r->gate.registered) { aros_break_close(&r->gate);return TTY_UNSUPPORTED; }
    /* Caller keeps target launch-gated/alive through this registration. */
    aros_break_bind(&r->gate,task);
    r->service=s;r->delivered=r->dropped=0;
    ++s->signal_generation;
    s->interrupt_context=r;s->interrupt=deliver_interrupt;
    return TTY_OK;
}
void aros_tty_signal_close(ArosTtySignal *r)
{
    TtyPair *p;
    if(!r || !r->service) return;
    r->service->interrupt=NULL;r->service->interrupt_context=NULL;
    if(r->service->signal_generation!=UINT64_MAX) ++r->service->signal_generation;
    p=&r->service->pair;
    tty_pair_take_interrupt(p); /* Never retarget an old pending event. */
    p->config.local_flags&=~(TTY_ISIG|TTY_NOFLSH);p->config.intr=0;
    ++p->config_generation;
    aros_break_close(&r->gate); /* Waits for any in-flight removal callback. */
    r->service=NULL;
}
