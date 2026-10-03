#ifndef XTERM_FLOOD_CLOCK_H
#define XTERM_FLOOD_CLOCK_H
#include <stdint.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
typedef struct {
    struct MsgPort *port;
    struct timerequest *io;
    uint64_t last;
    ULONG hz;
    int bad, opened;
} FloodClock;
static int flood_clock_open(FloodClock *c)
{
    c->port=CreateMsgPort();
    if(c->port) c->io=(struct timerequest *)CreateIORequest(c->port,sizeof *c->io);
    if(!c->io || OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)c->io,0)) return 0;
    c->opened=1;return 1;
}
static uint64_t flood_now(FloodClock *c)
{
    struct Device *TimerBase=c->io->tr_node.io_Device;
    struct EClockVal e; ULONG hz=ReadEClock(&e);
    uint64_t t=((uint64_t)e.ev_hi<<32)|e.ev_lo;
    if(!hz || (c->hz && c->hz!=hz) || t<c->last) c->bad=1;
    c->hz=hz;c->last=t;return t;
}
static void flood_clock_close(FloodClock *c)
{
    if(c->opened) CloseDevice((struct IORequest *)c->io);
    if(c->io) DeleteIORequest((struct IORequest *)c->io);
    if(c->port) DeleteMsgPort(c->port);
}
#endif
