/* DIAGNOSTIC ONLY: see perf_diag.h. Never part of a release build. */
#ifdef XTERM_PERF_DIAG
#include "perf_diag.h"

#include <stdio.h>
#include <string.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>

PerfDiag perf_diag;
/* Shared with the redraw timer in aros_xterm.c. */
extern struct Device *TimerBase;
static struct timerequest perf_timer;
static int perf_timer_open;
static BPTR perf_file;
static uint64_t perf_started;
static unsigned long perf_delay_ms,perf_spike_ms;

static unsigned long perf_var(CONST_STRPTR name)
{
    UBYTE value[16]; unsigned long ms=0; LONG i,n;
    n=GetVar(name,value,sizeof value,GVF_GLOBAL_ONLY);
    for(i=0;i<n && value[i]>='0' && value[i]<='9';++i) ms=ms*10+(value[i]-'0');
    return n>0?ms:0;
}
static void perf_read_vars(void)
{
    unsigned long spike;
    perf_delay_ms=perf_var((CONST_STRPTR)"XTERM_PERF_DRAW_DELAY_MS");
    if((spike=perf_var((CONST_STRPTR)"XTERM_PERF_DRAW_SPIKE_MS"))) {
        perf_spike_ms=spike; DeleteVar((CONST_STRPTR)"XTERM_PERF_DRAW_SPIKE_MS",GVF_GLOBAL_ONLY);
    }
}
void perf_draw_delay(void)
{
    unsigned long ms=perf_delay_ms;
    if(perf_spike_ms) { ms=perf_spike_ms; perf_spike_ms=0; }
    if(!ms) return;
    perf_diag.forced_delay_ms+=ms;
    Delay((ms+19)/20);
}

uint64_t perf_now(void)
{
    struct timeval tv;
    if(!perf_timer_open) return 0;
    GetSysTime(&tv);
    return (uint64_t)tv.tv_secs*1000000u+tv.tv_micro;
}

void perf_open(void)
{
    UBYTE path[256];
    memset(&perf_timer,0,sizeof perf_timer);
    if(!OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)&perf_timer,0)) {
        if(!TimerBase) TimerBase=perf_timer.tr_node.io_Device;
        perf_timer_open=1;
    }
    perf_read_vars();
    if(GetVar((CONST_STRPTR)"XTERM_PERF_LOG",path,sizeof path,GVF_GLOBAL_ONLY)>0 &&
       (perf_file=Open(path,MODE_READWRITE))) Seek(perf_file,0,OFFSET_END);
}

void perf_close(void)
{
    if(perf_file) { Close(perf_file); perf_file=0; }
    if(perf_timer_open) { CloseDevice((struct IORequest *)&perf_timer); perf_timer_open=0; }
}

void perf_tick(void)
{
    static const PerfDiag zero;
    PerfDiag *d=&perf_diag;
    char line[1800];
    uint64_t now=perf_now();
    if(!perf_started) { perf_started=now; return; }
    if(now-perf_started<1000000u) return;
    if(perf_file && (d->keys || d->write_packets || d->draws_with_rows)) {
        /* Two calls: the ABIv11 snprintf stub passes at most 19 variable
         * arguments (see the working notes), and a line has more. */
        size_t used;
        snprintf(line,sizeof line,
            "limpet t=%lu ms=%lu keys=%lu downs=%lu repeats=%lu key_latency_max=%lu key_queue_max=%lu "
            "input_backlog_max=%lu write_packets=%lu write_bytes=%lu write_waited=%lu write_wait_us=%lu "
            "write_wait_max=%lu output_backlog_max=%lu ",
            (unsigned long)(now/1000u),(unsigned long)((now-perf_started)/1000u),
            d->keys,d->downs,d->repeats,d->key_latency_max_us,d->key_queue_max,d->input_backlog_max,
            d->write_packets,d->write_bytes,d->write_waited,d->write_wait_us,d->write_wait_max_us,
            d->output_backlog_max);
        used=strlen(line);
        snprintf(line+used,sizeof line-used,
            "pumps=%lu pump_bytes=%lu parse_us=%lu draws=%lu draws_with_rows=%lu rows=%lu pixels=%lu "
            "raster_us=%lu upload_us=%lu draw_max=%lu loops=%lu loop_gap_max=%lu held=%lu settle_draws=%lu ",
            d->pumps,d->pump_bytes,d->parse_us,d->draws,d->draws_with_rows,d->rows,d->pixels,
            d->raster_us,d->upload_us,d->draw_max_us,d->loops,d->loop_gap_max_us,d->held,d->settle_draws);
        used=strlen(line);
        snprintf(line+used,sizeof line-used,
            "cache_hits=%lu cache_misses=%lu cache_replaced=%lu miss_us=%lu glyph_us=%lu ",
            d->cache_hits,d->cache_misses,d->cache_replaced,d->miss_us,d->glyph_us);
        used=strlen(line);
        snprintf(line+used,sizeof line-used,
            "draws_quiet=%lu draws_limit=%lu limit_max_ms=%lu forced_delay_ms=%lu ",
            d->draws_quiet,d->draws_limit,d->limit_max_ms,d->forced_delay_ms);
        used=strlen(line);
        snprintf(line+used,sizeof line-used,
            "model_checked=%lu model_torn_quiet=%lu model_torn_limit=%lu model_torn_other=%lu torn_sample=%s\n",
            d->model_checked,d->model_torn_quiet,d->model_torn_limit,d->model_torn_other,
            d->torn_sample[0]?d->torn_sample:"-");
        Write(perf_file,line,(LONG)strlen(line));
    }
    *d=zero; perf_started=now;
    perf_read_vars();
}
#else
/* ISO C wants at least one declaration in a translation unit. */
typedef int perf_diag_unused;
#endif
