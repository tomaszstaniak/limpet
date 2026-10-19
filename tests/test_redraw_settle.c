/* When held program output is drawn (src/redraw_settle.h). Times in us. */
#include "../src/redraw_settle.h"
#include <assert.h>
#include <stdio.h>

#define Q REDRAW_SETTLE_QUIET_US
#define L REDRAW_SETTLE_LIMIT_US

static RedrawSettle s;

static void single_write(void)
{
    redraw_settle_init(&s,1,Q,L);
    assert(!redraw_settle_due(&s,0) && redraw_settle_wake(&s,0)==0);
    assert(redraw_settle_output(&s,1000)==0);
    assert(redraw_settle_wake(&s,1000)==Q);
    assert(!redraw_settle_due(&s,1000+Q-1));
    assert(redraw_settle_due(&s,1000+Q));
}

/* A frame sent as writes 3 ms apart is drawn once, after its last write. */
static void burst_of_short_writes(void)
{
    uint64_t t;
    redraw_settle_init(&s,1,Q,L);
    for(t=0;t<=27000;t+=3000) {
        assert(!redraw_settle_due(&s,t));
        assert(redraw_settle_output(&s,t)==0);
    }
    assert(!redraw_settle_due(&s,27000+Q-1));
    assert(redraw_settle_due(&s,27000+Q));
}

/* Output that never pauses is still drawn by the limit, counted from the
 * first change not yet on screen; later writes do not move it. */
static void steady_stream(void)
{
    uint64_t t,drawn_at[4]; unsigned n=0;
    redraw_settle_init(&s,1,Q,L);
    for(t=0;t<350000 && n<4;t+=2000) {
        if(redraw_settle_due(&s,t)) { drawn_at[n++]=t; redraw_settle_drawn(&s); }
        redraw_settle_output(&s,t);
        assert(redraw_settle_wake(&s,t)<=L);
    }
    assert(n==3);
    assert(drawn_at[0]==L && drawn_at[1]==2*L && drawn_at[2]==3*L);
}

/* A wake-up that comes late draws at once; it never waits another period. */
static void late_wakeup(void)
{
    redraw_settle_init(&s,1,Q,L);
    redraw_settle_output(&s,0);
    assert(redraw_settle_due(&s,250000));
    assert(redraw_settle_wake(&s,250000)==0);
    /* Writes kept coming but the loop looked late: the limit has passed. */
    redraw_settle_init(&s,1,Q,L);
    redraw_settle_output(&s,0); redraw_settle_output(&s,L+5000);
    assert(redraw_settle_due(&s,L+5000));
}

/* A key or a resize drew the window: nothing stays held, and the next
 * output starts a new batch with its own limit. */
static void forced_draw_then_new_batch(void)
{
    redraw_settle_init(&s,1,Q,L);
    redraw_settle_output(&s,0); redraw_settle_output(&s,90000);
    redraw_settle_drawn(&s);
    assert(!redraw_settle_due(&s,90000+L) && redraw_settle_wake(&s,90000+L)==0);
    redraw_settle_output(&s,95000);
    redraw_settle_output(&s,104000);
    /* The old batch's limit (100000) no longer applies. */
    assert(!redraw_settle_due(&s,105000));
    assert(redraw_settle_wake(&s,104000)==Q);
    redraw_settle_output(&s,110000); redraw_settle_output(&s,120000);
    redraw_settle_output(&s,130000); redraw_settle_output(&s,140000);
    redraw_settle_output(&s,150000); redraw_settle_output(&s,160000);
    redraw_settle_output(&s,170000); redraw_settle_output(&s,180000);
    redraw_settle_output(&s,190000);
    assert(!redraw_settle_due(&s,194999));
    assert(redraw_settle_due(&s,95000+L));
}

static void without_timer(void)
{
    redraw_settle_init(&s,0,Q,L);
    assert(redraw_settle_output(&s,0)==1);
    assert(!redraw_settle_due(&s,1000000) && redraw_settle_wake(&s,0)==0);
}

static void limit_not_below_quiet(void)
{
    redraw_settle_init(&s,1,20000,5000);
    redraw_settle_output(&s,0);
    assert(!redraw_settle_due(&s,19999) && redraw_settle_due(&s,20000));
}

int main(void)
{
    single_write(); burst_of_short_writes(); steady_stream(); late_wakeup();
    forced_draw_then_new_batch(); without_timer(); limit_not_below_quiet();
    puts("redraw settle: ok");
    return 0;
}
