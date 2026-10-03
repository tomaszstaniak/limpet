#include "../src/aros_tty_termios.h"
#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ArosTtyWorker *worker;
static BPTR master,slave;
static struct MsgPort *reply;
static struct DosPacket *packet;
static int pending;
static unsigned char buffer[16];
static void fail(const char *expression,int line)
{
    printf("TTY READ TIMING NATIVE FAIL line=%d: %s\n",line,expression);fflush(stdout);
    if(worker) aros_tty_worker_stop(worker);
    if(pending) { WaitPort(reply);GetMsg(reply);pending=0; }
    if(slave) Close(slave);
    if(master) Close(master);
    if(worker && aros_tty_worker_finish(worker)) for(;;) Delay(50);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static ULONG ticks(void)
{
    struct DateStamp d;DateStamp(&d);
    return ((ULONG)d.ds_Days*1440+(ULONG)d.ds_Minute)*3000+(ULONG)d.ds_Tick;
}
static void begin_read(void)
{
    struct FileHandle *fh=BADDR(slave);
    CHECK(!pending && !GetMsg(reply));memset(buffer,0xa5,sizeof buffer);
    packet->dp_Type=ACTION_READ;packet->dp_Arg1=fh->fh_Arg1;
    packet->dp_Arg2=(SIPTR)buffer;packet->dp_Arg3=sizeof buffer;
    pending=1;SendPkt(packet,fh->fh_Type,reply);
    CHECK(DoPkt(fh->fh_Type,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
}
static void finish_read(LONG count,LONG error)
{
    unsigned i;struct Message *m=NULL;
    for(i=0;i<150 && !(m=GetMsg(reply));++i) Delay(1);
    CHECK(m==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==count && packet->dp_Res2==error);CHECK(!GetMsg(reply));
}
static void still_pending(void)
{
    struct Message *m=GetMsg(reply);if(m) pending=0;CHECK(!m);
}
int main(void)
{
    unsigned cycle;ULONG start,min_ticks=(ULONG)-1,max_ticks=0;struct termios t;
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);CHECK(reply && packet);
    for(cycle=0;cycle<10;++cycle) {
        worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
        CHECK(!aros_tty_tcgetattr(slave,&t));
        t.c_cc[VMIN]=0;t.c_cc[VTIME]=0;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        begin_read();finish_read(0,0);
        CHECK(Write(master,"x",1)==1);begin_read();finish_read(1,0);CHECK(buffer[0]=='x');
        t.c_cc[VMIN]=3;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        begin_read();CHECK(Write(master,"ab",2)==2);Delay(2);still_pending();
        CHECK(Write(master,"c",1)==1);finish_read(3,0);CHECK(!memcmp(buffer,"abc",3));
        t.c_cc[VMIN]=0;t.c_cc[VTIME]=2;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        start=ticks();begin_read();finish_read(0,0);start=ticks()-start;
        CHECK(start>=9 && start<150);if(start<min_ticks) min_ticks=start;if(start>max_ticks) max_ticks=start;
        begin_read();CHECK(Write(master,"x",1)==1);finish_read(1,0);CHECK(buffer[0]=='x');
        t.c_cc[VMIN]=3;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        begin_read();Delay(12);still_pending();
        CHECK(Write(master,"a",1)==1);Delay(4);still_pending();
        start=ticks();CHECK(Write(master,"b",1)==1);Delay(4);still_pending();
        finish_read(2,0);CHECK(ticks()-start>=9 && !memcmp(buffer,"ab",2));
        begin_read();CHECK(Write(master,"z",1)==1);
        t.c_cc[VMIN]=0;t.c_cc[VTIME]=0;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        finish_read(1,0);CHECK(buffer[0]=='z');
        t.c_cc[VMIN]=3;CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        begin_read();Delay(12);still_pending();
        CHECK(Close(slave));slave=BNULL;finish_read(-1,ERROR_BREAK);
        CHECK(!aros_tty_worker_stop(worker));CHECK(Close(master));master=BNULL;
        CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    }
    worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
    CHECK(!aros_tty_tcgetattr(slave,&t));t.c_cc[VMIN]=0;t.c_cc[VTIME]=20;
    CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));begin_read();
    CHECK(!aros_tty_worker_stop(worker));finish_read(-1,ERROR_BREAK);
    CHECK(Close(slave));slave=BNULL;CHECK(Close(master));master=BNULL;
    CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    FreeDosObject(DOS_STDPKT,packet);DeleteMsgPort(reply);
    printf("TTY READ TIMING NATIVE PASS: cycles=10 cases=4 first_byte_wait interbyte_rearm mode_change stale_timer close stop reap timeout_ticks_min=%lu max=%lu\n",
           (unsigned long)min_ticks,(unsigned long)max_ticks);
    return 0;
}
