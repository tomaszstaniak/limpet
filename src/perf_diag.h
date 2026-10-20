/* DIAGNOSTIC ONLY (XTERM_PERF_DIAG): aggregated responsiveness counters.
 * One line per second of activity goes to the file named by the global
 * variable XTERM_PERF_LOG. Nothing is written per key, packet or row.
 * Without XTERM_PERF_DIAG every hook is an empty macro. */
#ifndef PERF_DIAG_H
#define PERF_DIAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef XTERM_PERF_DIAG
typedef struct {
    unsigned long keys,downs,repeats,key_latency_max_us;
    unsigned long key_queue_max,input_backlog_max;
    unsigned long write_packets,write_bytes,write_waited,write_wait_us,write_wait_max_us;
    unsigned long output_backlog_max;
    unsigned long pumps,pump_bytes,parse_us;
    unsigned long draws,draws_with_rows,rows,pixels,raster_us,upload_us,draw_max_us;
    unsigned long loops,loop_gap_max_us;
    unsigned long held,settle_draws;
    unsigned long cache_hits,cache_misses,cache_replaced,miss_us,glyph_us;
    unsigned long draws_quiet,draws_limit,limit_max_ms,forced_delay_ms;
    /* Draws whose screen model had line numbers in the first columns
     * (Helix's gutter) out of order: a frame not fully received yet. By
     * what made the draw: settle quiet, settle limit, anything else. */
    unsigned long model_checked,model_torn_quiet,model_torn_limit,model_torn_other;
} PerfDiag;
extern PerfDiag perf_diag;
uint64_t perf_now(void);
void perf_open(void);
void perf_close(void);
/* Writes and resets the counters once a second has passed. */
void perf_tick(void);
/* Test of the redraw schedule only: lengthens a draw by the milliseconds in
 * XTERM_PERF_DRAW_DELAY_MS (every draw) or XTERM_PERF_DRAW_SPIKE_MS (the
 * next draw once; the variable is deleted). Read once a second. Not a
 * benchmark: it makes draws slow on purpose. */
void perf_draw_delay(void);
#define PERF_NOW() perf_now()
#define PERF_DO(statement) do { statement; } while(0)
#define PERF_MAX(field,value) do { unsigned long v_=(unsigned long)(value); \
    if(v_>perf_diag.field) perf_diag.field=v_; } while(0)
#else
#define PERF_NOW() 0
#define PERF_DO(statement) do {} while(0)
#define PERF_MAX(field,value) do {} while(0)
#endif

#endif
