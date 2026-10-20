/* When program output is drawn. A program sends one screen as many writes
 * and a terminal does not know where a frame ends, so output is drawn when
 * it pauses (`quiet`) and at the latest `limit` after the first change not
 * yet on screen. Later output never moves that limit, so a steady stream is
 * still drawn regularly.
 *
 * When drawing is slow the limit stretches to twice the last draw's cost, up
 * to `stretch_max`. With steady output and draws of similar cost, drawing
 * then takes about a third of the time and the program producing the output
 * keeps the rest; that is the intent, not a bound: a pause in the output, a
 * key or resize that draws at once, or a draw costing more or less than the
 * last one all change the share. Measured on a slow AROS guest, drawing
 * every 100 ms made a program catching up on held keys take twice as long.
 * The cap keeps one outlier (a guest paused in the middle of a draw) from
 * delaying the next steady output by seconds; the following draw's cost
 * sets the limit again.
 *
 * Times are microseconds of one monotonic clock that the caller reads;
 * nothing here waits or reads a clock itself. */
#ifndef REDRAW_SETTLE_H
#define REDRAW_SETTLE_H

#include <stdint.h>

#define REDRAW_SETTLE_QUIET_US 12000u
#define REDRAW_SETTLE_LIMIT_US 100000u
/* Draws on the slowest measured guest took 25-110 ms for a full window, so
 * twice their cost stays below this; it still shows a steady stream at
 * least about twice a second when a draw takes up to 200 ms. */
#define REDRAW_SETTLE_STRETCH_MAX_US 400000u

typedef struct {
    /* 0: no timer, every output is drawn at once. */
    int timed;
    int held;
    uint64_t first,last;
    uint32_t quiet,limit,stretch_max;
    /* What the last draw took. */
    uint32_t cost;
} RedrawSettle;

void redraw_settle_init(RedrawSettle *,int timed,uint32_t quiet_us,uint32_t limit_us,
                        uint32_t stretch_max_us);
/* Output that changes the screen arrived at `now`. 1: draw now (no timer);
 * 0: held, ask redraw_settle_wake for when to look again. */
int redraw_settle_output(RedrawSettle *,uint64_t now);
/* 1 when held output is due at `now`. Late calls are fine: a late look
 * draws at once, it never waits another period. */
int redraw_settle_due(const RedrawSettle *,uint64_t now);
/* Microseconds from `now` until held output is due (0: due or nothing held). */
uint64_t redraw_settle_wake(const RedrawSettle *,uint64_t now);
/* The limit that applies now: `limit`, or twice the last draw's cost up to
 * `stretch_max`. */
uint32_t redraw_settle_limit(const RedrawSettle *);
/* The window was drawn, for whatever reason, in `cost_us`: the next output
 * starts anew. */
void redraw_settle_drawn(RedrawSettle *,uint32_t cost_us);

#endif
