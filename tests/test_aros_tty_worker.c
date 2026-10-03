#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyWorker *workers[2];
static BPTR master[2],slave[2];
static struct MsgPort *reply;
static struct DosPacket *packet;
static int pending;
static void fail(const char *expr,int line)
{
    unsigned i;
    printf("TTY WORKER FAIL line=%d: %s\n",line,expr);fflush(stdout);
    for(i=0;i<2;++i) if(workers[i]) aros_tty_worker_stop(workers[i]);
    if(pending) { WaitPort(reply);GetMsg(reply);pending=0; }
    for(i=0;i<2;++i) {
        if(master[i]) Close(master[i]);
        if(slave[i]) Close(slave[i]);
        if(workers[i] && aros_tty_worker_finish(workers[i])) {
            puts("TTY WORKER retaining unresolved process ownership");fflush(stdout);
            for(;;) Delay(50);
        }
    }
    if(packet) FreeDosObject(DOS_STDPKT,packet);
    if(reply) DeleteMsgPort(reply);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void close_pair(unsigned i)
{
    BPTR fh=master[i];master[i]=BNULL;CHECK(Close(fh));
    fh=slave[i];slave[i]=BNULL;CHECK(Close(fh));
    CHECK(!aros_tty_worker_finish(workers[i]));workers[i]=NULL;
}
int main(void)
{
    unsigned cycle;unsigned char b[8];
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);CHECK(reply && packet);
    for(cycle=0;cycle<32;++cycle) {
        struct FileHandle *fh;
        workers[0]=aros_tty_worker_start(&master[0],&slave[0]);CHECK(workers[0]);
        workers[1]=aros_tty_worker_start(&master[1],&slave[1]);CHECK(workers[1]);
        CHECK(aros_tty_worker_finish(workers[0])==-1);
        CHECK(Write(master[0],"a\0b",3)==3);
        CHECK(Write(master[1],"xyz",3)==3);
        CHECK(Read(slave[1],b,3)==3 && !memcmp(b,"xyz",3));
        CHECK(Read(slave[0],b,3)==3 && !memcmp(b,"a\0b",3));
        CHECK(Write(slave[0],"out",3)==3);
        CHECK(Read(master[0],b,3)==3 && !memcmp(b,"out",3));
        close_pair(1);
        /* No parent polling: the worker alone owns and services this packet. */
        fh=BADDR(slave[0]);packet->dp_Type=ACTION_READ;
        packet->dp_Arg1=fh->fh_Arg1;packet->dp_Arg2=(SIPTR)b;packet->dp_Arg3=1;
        pending=1;SendPkt(packet,fh->fh_Type,reply);
        CHECK(!aros_tty_worker_stop(workers[0]));
        WaitPort(reply);CHECK(GetMsg(reply)==packet->dp_Link);pending=0;
        CHECK(packet->dp_Res1==-1 && packet->dp_Res2==ERROR_BREAK);
        CHECK(aros_tty_worker_finish(workers[0])==-1);
        CHECK(Write(master[0],"x",1)==-1 && IoErr()==ERROR_BREAK);
        close_pair(0);
    }
    FreeDosObject(DOS_STDPKT,packet);DeleteMsgPort(reply);
    puts("TTY WORKER PASS: cycles=32 workers=64 ordinary DOS I/O, isolation, busy finish, stop, Close, reap");
    return 0;
}
