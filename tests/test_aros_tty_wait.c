#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyWorker *worker;
static BPTR handles[3];
static struct MsgPort *reply[2];
static struct DosPacket *packet[2];
static int pending[2];
static void fail(const char *expr,int line)
{
    unsigned i;
    printf("TTY WAIT FAIL line=%d: %s\n",line,expr);fflush(stdout);
    if(worker) aros_tty_worker_stop(worker);
    for(i=0;i<2;++i) if(pending[i]) { WaitPort(reply[i]);GetMsg(reply[i]); }
    for(i=0;i<3;++i) if(handles[i]) Close(handles[i]);
    if(worker && aros_tty_worker_finish(worker)) for(;;) Delay(50);
    for(i=0;i<2;++i) {
        if(packet[i]) FreeDosObject(DOS_STDPKT,packet[i]);
        if(reply[i]) DeleteMsgPort(reply[i]);
    }
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static struct MsgPort *port(unsigned end)
{ return ((struct FileHandle *)BADDR(handles[end]))->fh_Type; }
static void start_wait(unsigned end,LONG timeout)
{
    packet[end]->dp_Type=ACTION_WAIT_CHAR;packet[end]->dp_Arg1=timeout;
    pending[end]=1;SendPkt(packet[end],port(end),reply[end]);
    /* Same-port barrier: prove admission, not merely enqueueing. */
    CHECK(DoPkt(port(end),ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
}
static void collect(unsigned end,LONG value,LONG error)
{
    unsigned tick;struct Message *m=NULL;
    for(tick=0;tick<500 && !(m=GetMsg(reply[end]));++tick) Delay(1);
    CHECK(m==packet[end]->dp_Link);pending[end]=0;
    CHECK(packet[end]->dp_Res1==value && packet[end]->dp_Res2==error);
}
static ULONG ticks(void)
{ struct DateStamp d;DateStamp(&d);return (ULONG)d.ds_Minute*60*TICKS_PER_SECOND+d.ds_Tick; }
static void close_handle(unsigned i)
{ BPTR h=handles[i];handles[i]=BNULL;CHECK(Close(h)); }
int main(void)
{
    unsigned i;unsigned char b[8];ULONG started,elapsed;
    for(i=0;i<2;++i) {
        reply[i]=CreateMsgPort();packet[i]=AllocDosObject(DOS_STDPKT,NULL);
        CHECK(reply[i] && packet[i]);
    }
    worker=aros_tty_worker_start(&handles[0],&handles[1]);CHECK(worker);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE && IoErr()==0);
    CHECK(WaitForChar(handles[0],-1)==DOSFALSE && IoErr()==ERROR_BAD_NUMBER);
    CHECK(Write(handles[0],"\0",1)==1);
    CHECK(WaitForChar(handles[1],0)==DOSTRUE);
    CHECK(WaitForChar(handles[0],0)==DOSFALSE);
    CHECK(Read(handles[1],b,sizeof b)==1 && b[0]==0);
    started=ticks();
    CHECK(WaitForChar(handles[1],500000)==DOSFALSE && IoErr()==0);
    elapsed=ticks()-started;CHECK(elapsed>=24 && elapsed<500);
    printf("TTY WAIT TIMEOUT PASS: requested_us=500000 elapsed_ticks=%lu hz=%d\n",
           (unsigned long)elapsed,TICKS_PER_SECOND);
    for(i=0;i<32;++i) {
        unsigned end=i%2;
        start_wait(end,5000000);CHECK(!GetMsg(reply[end]));
        CHECK(WaitForChar(handles[end],0)==DOSFALSE && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(Read(handles[end],b,1)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(Write(handles[1-end],"x",1)==1);
        collect(end,DOSTRUE,0);
        CHECK(Read(handles[end],b,sizeof b)==1 && b[0]=='x');
    }
    packet[1]->dp_Type=ACTION_READ;
    packet[1]->dp_Arg1=((struct FileHandle *)BADDR(handles[1]))->fh_Arg1;
    packet[1]->dp_Arg2=(SIPTR)b;packet[1]->dp_Arg3=1;
    pending[1]=1;SendPkt(packet[1],port(1),reply[1]);
    CHECK(DoPkt(port(1),ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE && IoErr()==ERROR_OBJECT_IN_USE);
    CHECK(Write(handles[0],"r",1)==1);collect(1,1,0);CHECK(b[0]=='r');
    start_wait(0,200000);start_wait(1,5000000);
    collect(0,DOSFALSE,0);CHECK(!GetMsg(reply[1]));
    CHECK(Write(handles[0],"t",1)==1);collect(1,DOSTRUE,0);
    CHECK(Read(handles[1],b,sizeof b)==1 && b[0]=='t');
    CHECK(SetMode(handles[1],0)==DOSTRUE);
    start_wait(1,5000000);
    CHECK(Write(handles[0],"a",1)==1);
    CHECK(!GetMsg(reply[1])); /* Uncommitted canonical input is not readable. */
    CHECK(Write(handles[0],"\n",1)==1);collect(1,DOSTRUE,0);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"a\n",2));
    CHECK(Read(handles[0],b,sizeof b)==2 && !memcmp(b,"a\n",2));
    CHECK(Write(handles[0],"\004z\n",3)==3);
    CHECK(WaitForChar(handles[1],5000000)==DOSFALSE && IoErr()==0);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE); /* Does not consume VEOF. */
    CHECK(Read(handles[1],b,sizeof b)==0);
    CHECK(WaitForChar(handles[1],0)==DOSTRUE);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"z\n",2));
    CHECK(Read(handles[0],b,sizeof b)==2 && !memcmp(b,"z\n",2));
    CHECK(SetMode(handles[1],1)==DOSTRUE);
    start_wait(0,5000000);start_wait(1,5000000);
    CHECK(!GetMsg(reply[0]) && !GetMsg(reply[1]));
    CHECK(!aros_tty_worker_stop(worker));
    collect(0,DOSFALSE,ERROR_BREAK);collect(1,DOSFALSE,ERROR_BREAK);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE && IoErr()==ERROR_BREAK);
    close_handle(0);close_handle(1);
    CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    worker=aros_tty_worker_start(&handles[0],&handles[1]);CHECK(worker);
    CHECK(aros_tty_worker_duplicate(worker,handles[0],&handles[2])==TTY_OK);
    start_wait(0,5000000);start_wait(1,5000000);
    close_handle(2);CHECK(!GetMsg(reply[0]) && !GetMsg(reply[1]));
    close_handle(0);
    collect(0,DOSFALSE,ERROR_BREAK);collect(1,DOSFALSE,0);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE && IoErr()==0);
    close_handle(1);CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    worker=aros_tty_worker_start(&handles[0],&handles[1]);CHECK(worker);
    {
        TtyConfig c;
        CHECK(aros_tty_worker_get_config(worker,1,&c)==TTY_OK);
        c.local_flags=TTY_CANON;
        CHECK(aros_tty_worker_configure(worker,&c)==TTY_OK);
    }
    start_wait(1,5000000);
    CHECK(Write(handles[0],"tail",4)==4);
    CHECK(!GetMsg(reply[1]));
    close_handle(0);
    collect(1,DOSTRUE,0); /* Hangup publishes the unterminated line as data. */
    CHECK(Read(handles[1],b,2)==2 && !memcmp(b,"ta",2));
    CHECK(WaitForChar(handles[1],0)==DOSTRUE);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"il",2));
    CHECK(WaitForChar(handles[1],0)==DOSFALSE && IoErr()==0);
    CHECK(Read(handles[1],b,sizeof b)==0);
    CHECK(!GetMsg(reply[1]));
    close_handle(1);CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    for(i=0;i<2;++i) { FreeDosObject(DOS_STDPKT,packet[i]);DeleteMsgPort(reply[i]); }
    puts("TTY WAIT PASS: native timeout, binary readiness, 32 timer reuses, endpoint isolation, busy readers, canonical records, temporary EOF, duplicate/final close, dual stop, reap");
    return 0;
}
