/* A single async writer serializes flood, acknowledgements and the trailer.
 * The independent read packet must remain serviceable while that writer waits. */
#include "flood_peer.h"
#include "flood_protocol.h"
#include "flood_clock.h"
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t seq,tick; } Ack;
typedef struct {
    FloodParser parser;
    FloodClock clock;
    Ack acks[2048]; unsigned head,count;
    uint64_t pause_until,frames,last_ack;
    int stop,failed;
} Peer;
static int received(void *context,unsigned op,uint64_t seq,uint64_t unused)
{
    Peer *p=context; uint64_t t=flood_now(&p->clock); unsigned at;
    (void)unused;
    if(p->count==2048 || p->clock.bad) return 0;
    at=(p->head+p->count)%2048;p->acks[at].seq=seq;p->acks[at].tick=t;++p->count;
    if(op==FLOOD_PAUSE) p->pause_until=t+p->clock.hz/4;
    if(op==FLOOD_STOP) p->stop=1;
    return 1;
}
static void submit(struct DosPacket *pkt,BPTR handle,struct MsgPort *reply,
                   LONG action,void *bytes,size_t length)
{
    struct FileHandle *fh=BADDR(handle);
    pkt->dp_Type=action;pkt->dp_Arg1=fh->fh_Arg1;
    pkt->dp_Arg2=(IPTR)bytes;pkt->dp_Arg3=length;
    SendPkt(pkt,fh->fh_Type,reply);
}
int flood_peer(void)
{
    Peer *p=calloc(1,sizeof *p);
    struct MsgPort *rp=NULL,*wp=NULL;
    struct DosPacket *readpkt=NULL,*writepkt=NULL;
    unsigned char in[16],out[16384];size_t length=0,offset=0;
    int reading=0,writing=0,trailer=0,raw=0,result=20;
    if(!p) return 20;
    rp=CreateMsgPort();wp=CreateMsgPort();
    readpkt=AllocDosObject(DOS_STDPKT,NULL);writepkt=AllocDosObject(DOS_STDPKT,NULL);
    if(!rp || !wp || !readpkt || !writepkt || !flood_clock_open(&p->clock)) goto cleanup;
    if(IsInteractive(Input())) { if(!SetMode(Input(),1)) goto cleanup;raw=1; }
    for(;;) {
        uint64_t now=flood_now(&p->clock);
        if(p->clock.bad) p->failed=1;
        if(reading && GetMsg(rp)) {
            LONG n=readpkt->dp_Res1;reading=0;
            if(n<=0 || n>(LONG)sizeof in || !flood_input_feed(&p->parser,in,n,received,p)) p->failed=1;
        }
        if(writing && GetMsg(wp)) {
            LONG n=writepkt->dp_Res1;writing=0;
            if(n<=0 || (size_t)n>length-offset) { p->failed=1;length=offset=0; }
            else offset+=(size_t)n;
        }
        if(!writing && offset<length && !p->failed) {
            submit(writepkt,Output(),wp,ACTION_WRITE,out+offset,length-offset);writing=1;
        }
        if(p->failed) {
            /* Signal the owner through process exit only after it has cancelled
             * outstanding packets. The diagnostic parent has a watchdog. */
            if(!reading && !writing) break;
        } else {
            if(!reading && !p->stop && now>=p->pause_until) {
                submit(readpkt,Input(),rp,ACTION_READ,in,sizeof in);reading=1;
            }
            if(!writing && offset==length) {
                if(trailer) { result=0;break; }
                length=offset=0;
                if(p->count) {
                    while(p->count) {
                        Ack *a=&p->acks[p->head];
                        size_t n=flood_frame(out+length,sizeof out-length,'A',a->seq,a->tick);
                        if(!n) break;
                        length+=n;p->last_ack=a->seq;p->head=(p->head+1)%2048;--p->count;
                    }
                } else if(p->stop) {
                    length=flood_frame(out,sizeof out,'T',p->last_ack,0);trailer=1;
                } else {
                    for(;;) {
                        size_t n=flood_frame(out+length,sizeof out-length,'F',p->frames+1,0);
                        if(!n) break;
                        ++p->frames;length+=n;
                    }
                }
                if(!length) { p->failed=1;continue; }
                submit(writepkt,Output(),wp,ACTION_WRITE,out,length);writing=1;
            }
        }
        if(!reading && !p->stop && !p->failed && now<p->pause_until) Delay(1);
        else Wait((1UL<<rp->mp_SigBit)|(1UL<<wp->mp_SigBit));
    }
    if(raw && !SetMode(Input(),0)) result=20;
cleanup:
    flood_clock_close(&p->clock);
    if(readpkt) FreeDosObject(DOS_STDPKT,readpkt);
    if(writepkt) FreeDosObject(DOS_STDPKT,writepkt);
    if(rp) DeleteMsgPort(rp);
    if(wp) DeleteMsgPort(wp);
    free(p);return result;
}
