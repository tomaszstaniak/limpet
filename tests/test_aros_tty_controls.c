#include "../src/aros_tty_worker.h"
#include "../src/tty_control.h"
#include "../src/aros_tty_client.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyWorker *workers[2];
static BPTR handles[16],other_master,other_slave;
static struct MsgPort *reply;
static struct DosPacket *packet;
static int pending;
static void fail(const char *expr,int line)
{
    unsigned i;
    printf("TTY CONTROLS FAIL line=%d: %s\n",line,expr);fflush(stdout);
    for(i=0;i<2;++i) if(workers[i]) aros_tty_worker_stop(workers[i]);
    if(pending) { WaitPort(reply);GetMsg(reply);pending=0; }
    for(i=0;i<16;++i) if(handles[i]) Close(handles[i]);
    if(other_master) Close(other_master);
    if(other_slave) Close(other_slave);
    for(i=0;i<2;++i) if(workers[i] && aros_tty_worker_finish(workers[i])) {
        puts("TTY CONTROLS retaining unresolved ownership");fflush(stdout);
        for(;;) Delay(50);
    }
    if(packet) FreeDosObject(DOS_STDPKT,packet);
    if(reply) DeleteMsgPort(reply);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void close_handle(BPTR *h)
{ BPTR old=*h;*h=BNULL;CHECK(Close(old)); }
/* Actual DOS transport, shared flags and canonical delimiter order. */
static void input_mapping(void)
{
    static const char *want[]={"\r\n","\n\n","\n","\n","\r\r","\n\r","\r","\r"};
    unsigned flags;unsigned char b[8];XtyControl c;
    for(flags=0;flags<8;++flags) {
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;
        c.values[0]=flags;c.values[2]=XTY_ECHO;c.values[3]=4;
        CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
        CHECK(c.values[0]==flags);
        c.operation=XTY_GET_CONFIG;CHECK(aros_tty_control(handles[0],&c)==DOSTRUE);
        CHECK(c.values[0]==flags);
        CHECK(Write(handles[0],"\r\n",2)==2);
        CHECK(WaitForChar(handles[1],1000000)==DOSTRUE);
        CHECK(Read(handles[1],b,sizeof b)==(LONG)strlen(want[flags]));
        CHECK(!memcmp(b,want[flags],strlen(want[flags])));
        CHECK(WaitForChar(handles[0],1000000)==DOSTRUE);
        CHECK(Read(handles[0],b,sizeof b)==(LONG)strlen(want[flags]));
        CHECK(!memcmp(b,want[flags],strlen(want[flags])));
    }
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;
    c.values[0]=5;c.values[2]=XTY_CANON;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(Write(handles[0],"A\n",2)==2);
    CHECK(WaitForChar(handles[1],0)==DOSFALSE);
    CHECK(Write(handles[0],"\r",1)==1);
    CHECK(WaitForChar(handles[1],1000000)==DOSTRUE);
    CHECK(Read(handles[1],b,sizeof b)==3 && !memcmp(b,"A\r\n",3));
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    puts("TTY INPUT MAPPING NATIVE PASS: combinations=8 echo=8 canonical=1");fflush(stdout);
}
static void read_state(BPTR handle,ULONG expected)
{
    XtyControl control;unsigned repeat,i;
    memset(&control,0,sizeof control);control.version=1;control.operation=XTY_GET_READ_STATE;
    for(repeat=0;repeat<2;++repeat) {
        CHECK(aros_tty_control(handle,&control)==DOSTRUE);
        CHECK(control.values[0]==expected);
        for(i=1;i<12;++i) CHECK(control.values[i]==0);
    }
}
static void flush_queues(unsigned mask)
{
    XtyControl c;unsigned i;
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_FLUSH;c.values[0]=mask;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    for(i=0;i<12;++i) CHECK(c.values[i]==0);
}
static void flushing(void)
{
    unsigned mask,direction,tick;unsigned char b[16],full[TTY_QUEUE_CAPACITY];
    XtyControl c;struct MsgPort *port;struct Message *completed;
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_CAPS;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(c.values[0]&XTY_CAP_FLUSH);CHECK((c.values[1]&7)==7);
    c.operation=XTY_CAPS;CHECK(aros_tty_control(handles[0],&c)==DOSTRUE);
    CHECK(!(c.values[0]&XTY_CAP_FLUSH));
    c.operation=XTY_FLUSH;memset(c.values,0,sizeof c.values);c.values[0]=1;
    CHECK(aros_tty_control(handles[0],&c)==DOSFALSE && IoErr()==ERROR_ACTION_NOT_KNOWN);
    for(mask=1;mask<=3;++mask) {
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;
        c.values[2]=XTY_CANON;c.values[3]=4;
        CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
        CHECK(Write(handles[0],"old\n\004tail",9)==9);
        CHECK(Write(handles[1],"output",6)==6);
        flush_queues(mask);
        if(mask&1) {
            read_state(handles[1],XTY_READ_WAIT);
            CHECK(Write(handles[0],"new\n",4)==4);
            CHECK(Read(handles[1],b,sizeof b)==4 && !memcmp(b,"new\n",4));
        } else {
            CHECK(Read(handles[1],b,sizeof b)==4 && !memcmp(b,"old\n",4));
            CHECK(Read(handles[1],b,sizeof b)==0);
            CHECK(Write(handles[0],"\n",1)==1);
            CHECK(Read(handles[1],b,sizeof b)==5 && !memcmp(b,"tail\n",5));
        }
        if(mask&2) read_state(handles[0],XTY_READ_WAIT);
        else CHECK(Read(handles[0],b,sizeof b)==6 && !memcmp(b,"output",6));
    }
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    memset(full,'f',sizeof full);
    for(direction=0;direction<2;++direction) {
        BPTR writer=handles[direction],reader=handles[1-direction];
        CHECK(Write(writer,full,sizeof full)==sizeof full);
        packet->dp_Type=ACTION_WRITE;
        packet->dp_Arg1=((struct FileHandle *)BADDR(writer))->fh_Arg1;
        packet->dp_Arg2=(SIPTR)"new";packet->dp_Arg3=3;
        port=((struct FileHandle *)BADDR(writer))->fh_Type;
        pending=1;SendPkt(packet,port,reply);
        CHECK(DoPkt(port,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
        CHECK(!GetMsg(reply)); /* Barrier: writer is blocked before flush. */
        flush_queues(direction?XTY_FLUSH_OUTPUT:XTY_FLUSH_INPUT);
        completed=NULL;
        for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
        CHECK(completed==packet->dp_Link);pending=0;
        CHECK(packet->dp_Res1==3 && packet->dp_Res2==0);
        CHECK(WaitForChar(reader,1000000)==DOSTRUE);
        CHECK(Read(reader,b,sizeof b)==3 && !memcmp(b,"new",3));
        read_state(reader,XTY_READ_WAIT);
        CHECK(!GetMsg(reply));
    }
    puts("TTY FLUSH NATIVE PASS: directions=3 canonical/EOF, capabilities, blocked_writers=2");fflush(stdout);
}
static void configure_discard(void)
{
    XtyControl c,before;unsigned char full[TTY_QUEUE_CAPACITY],b[TTY_QUEUE_CAPACITY];
    unsigned tick;struct MsgPort *port;struct Message *completed;
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_CAPS;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(c.values[0]&XTY_CAP_SET_CONFIG_DISCARD_INPUT);
    c.operation=XTY_CAPS;CHECK(aros_tty_control(handles[0],&c)==DOSTRUE);
    CHECK(!(c.values[0]&XTY_CAP_SET_CONFIG_DISCARD_INPUT));
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[2]=3;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(Write(handles[0],"old\n\004tail",9)==9);
    CHECK(Read(handles[0],b,sizeof b)==8 && !memcmp(b,"old\ntail",8));
    memset(full,'f',sizeof full);CHECK(Write(handles[1],full,sizeof full)==sizeof full);
    packet->dp_Type=ACTION_WRITE;
    packet->dp_Arg1=((struct FileHandle *)BADDR(handles[0]))->fh_Arg1;
    packet->dp_Arg2=(SIPTR)"\n\004";packet->dp_Arg3=2;
    port=((struct FileHandle *)BADDR(handles[0]))->fh_Type;
    pending=1;SendPkt(packet,port,reply);
    CHECK(DoPkt(port,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);CHECK(!GetMsg(reply));
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG_DISCARD_INPUT;
    c.values[0]=XTY_INLCR;c.values[3]='\n';before=c;
    CHECK(aros_tty_control(handles[1],&c)==DOSFALSE && IoErr()==ERROR_BAD_NUMBER);
    CHECK(!memcmp(&c,&before,sizeof c));read_state(handles[1],XTY_READ_DATA);
    CHECK(!GetMsg(reply));c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(c.values[0]==XTY_INLCR && c.values[2]==0);
    completed=NULL;
    for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
    CHECK(completed==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==2 && packet->dp_Res2==0);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"\r\004",2));
    read_state(handles[1],XTY_READ_WAIT);
    CHECK(Read(handles[0],b,sizeof b)==sizeof b && !memcmp(b,full,sizeof b));
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    puts("TTY CONFIG DISCARD NATIVE PASS: atomic validation/input discard, pending writer new mode, output retained");fflush(stdout);
}
static void mode_preservation(void)
{
    unsigned path,tick;XtyControl c;TtyConfig raw;
    unsigned char b[TTY_QUEUE_CAPACITY],full[TTY_QUEUE_CAPACITY];
    struct Message *completed;struct FileHandle *fh=BADDR(handles[1]);
    CHECK(aros_tty_worker_get_config(workers[0],1,&raw)==TTY_OK);
    for(path=0;path<3;++path) {
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[2]=XTY_CANON;c.values[3]=4;
        CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
        CHECK(Write(handles[0],"tail",4)==4);
        packet->dp_Type=ACTION_READ;packet->dp_Arg1=fh->fh_Arg1;
        packet->dp_Arg2=(SIPTR)b;packet->dp_Arg3=sizeof b;
        pending=1;SendPkt(packet,fh->fh_Type,reply);
        CHECK(DoPkt(fh->fh_Type,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
        CHECK(!GetMsg(reply));
        if(path==0) CHECK(SetMode(handles[1],1)==DOSTRUE);
        else if(path==1) {
            memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[3]=4;
            CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
        } else CHECK(aros_tty_worker_configure(workers[0],&raw)==TTY_OK);
        completed=NULL;
        for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
        CHECK(completed==packet->dp_Link);pending=0;
        CHECK(packet->dp_Res1==4 && !packet->dp_Res2 && !memcmp(b,"tail",4));
        CHECK(!GetMsg(reply));
    }
    memset(full,'q',sizeof full);CHECK(Write(handles[0],full,sizeof full)==sizeof full);
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[2]=XTY_CANON;c.values[3]=4;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    CHECK(Write(handles[0],"tail",4)==4);CHECK(SetMode(handles[1],1)==DOSTRUE);
    fh=BADDR(handles[0]);packet->dp_Type=ACTION_WRITE;packet->dp_Arg1=fh->fh_Arg1;
    packet->dp_Arg2=(SIPTR)"new";packet->dp_Arg3=3;
    pending=1;SendPkt(packet,fh->fh_Type,reply);
    CHECK(DoPkt(fh->fh_Type,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);CHECK(!GetMsg(reply));
    CHECK(Read(handles[1],b,3)==3 && !memcmp(b,"qqq",3));CHECK(!GetMsg(reply));
    CHECK(Read(handles[1],b,sizeof b)==sizeof full-3 && !memcmp(b,full,sizeof full-3));
    completed=NULL;
    for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
    CHECK(completed==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==3 && !packet->dp_Res2);
    CHECK(Read(handles[1],b,sizeof b)==4 && !memcmp(b,"tail",4));
    CHECK(Read(handles[1],b,sizeof b)==3 && !memcmp(b,"new",3));
    CHECK(WaitForChar(handles[1],0)==DOSFALSE);
    puts("TTY MODE NATIVE PASS: pending_read_paths=3 full_ring=1024 frozen_tail=4 ordered_writer=3");fflush(stdout);
}
static void drain_roundtrip(void)
{
    XtyControl c,before,reset;unsigned cycle,tick;unsigned char b[8];
    struct FileHandle *fh=BADDR(handles[1]);struct Message *completed;
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_CAPS;
    CHECK(aros_tty_control(handles[1],&c)==DOSTRUE && c.values[0]==16383);
    c.operation=XTY_CAPS;CHECK(aros_tty_control(handles[0],&c)==DOSTRUE && c.values[0]==8221);
    for(cycle=0;cycle<120;++cycle) {
        memset(&reset,0,sizeof reset);reset.version=1;reset.operation=XTY_SET_CONFIG;reset.values[3]=4;
        CHECK(aros_tty_control(handles[1],&reset)==DOSTRUE);
        CHECK(Write(handles[1],"old",3)==3);
        memset(&c,0,sizeof c);c.version=1;c.operation=9+cycle%3;
        if(c.operation!=9) { c.values[1]=3;c.values[3]=4; }
        if(c.operation==11) CHECK(Write(handles[0],"stale",5)==5);
        before=c;
        packet->dp_Type=XTY_ACTION_CONTROL;packet->dp_Arg1=fh->fh_Arg1;
        packet->dp_Arg2=(SIPTR)&c;packet->dp_Arg3=sizeof c;
        packet->dp_Arg4=XTY_PACKET_MAGIC;packet->dp_Arg5=0;
        pending=1;SendPkt(packet,fh->fh_Type,reply);
        CHECK(DoPkt(fh->fh_Type,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
        CHECK(!GetMsg(reply) && !memcmp(&c,&before,sizeof c));
        CHECK(Read(handles[0],b,2)==2 && !memcmp(b,"ol",2));
        CHECK(!GetMsg(reply));CHECK(Read(handles[0],b,1)==1 && b[0]=='d');
        completed=NULL;
        for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
        CHECK(completed==packet->dp_Link);pending=0;
        CHECK(packet->dp_Res1==DOSTRUE && !packet->dp_Res2);
        CHECK(c.reserved[0]==XTY_PACKET_MAGIC && c.values[1]==(c.operation==9?0:3));
        CHECK(!GetMsg(reply));read_state(handles[1],XTY_READ_WAIT);
        CHECK(Write(handles[1],"\n",1)==1);
        CHECK(Read(handles[0],b,sizeof b)==(c.operation==9?1:2));
        CHECK(!memcmp(b,c.operation==9?"\n":"\r\n",c.operation==9?1:2));
        /* The synchronous client also works on an already-empty barrier. */
        memset(&c,0,sizeof c);c.version=1;c.operation=9;
        CHECK(aros_tty_control(handles[1],&c)==DOSTRUE);
    }
    memset(&reset,0,sizeof reset);reset.version=1;reset.operation=XTY_SET_CONFIG;reset.values[3]=4;
    CHECK(aros_tty_control(handles[1],&reset)==DOSTRUE);
    puts("TTY DRAIN WORKER PASS: cycles=120 operations=3 partial reads, mode transition, discard, empty synchronous drain");fflush(stdout);
}
int main(void)
{
    unsigned i,tick,path;unsigned char b[8],full[TTY_QUEUE_CAPACITY];
    TtyConfig c,raw,got;TtyGeometry g,before;struct Message *completed;
    BPTR unchanged=(BPTR)123;SIPTR stale;struct MsgPort *port;
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);CHECK(reply && packet);
    workers[0]=aros_tty_worker_start(&handles[0],&handles[1]);CHECK(workers[0]);
    workers[1]=aros_tty_worker_start(&other_master,&other_slave);CHECK(workers[1]);
    input_mapping();flushing();configure_discard();mode_preservation();drain_roundtrip();
    {
        XtyControl control;struct FileHandle *fh=BADDR(handles[1]);
        memset(&control,0,sizeof control);control.version=1;control.operation=XTY_CAPS;
        CHECK(DoPkt(fh->fh_Type,XTY_ACTION_CONTROL,fh->fh_Arg1,(SIPTR)&control,
                    sizeof control,XTY_PACKET_MAGIC,0)==DOSTRUE);
        CHECK((control.values[0]&15)==15);
        CHECK(control.reserved[0]==XTY_PACKET_MAGIC);
        CHECK(control.values[0]&XTY_CAP_GET_READ_STATE);
        read_state(handles[0],XTY_READ_WAIT);read_state(handles[1],XTY_READ_WAIT);
        CHECK(Write(handles[0],"\0x",2)==2);
        read_state(handles[1],XTY_READ_DATA);read_state(handles[0],XTY_READ_WAIT);
        CHECK(Read(handles[1],b,1)==1 && b[0]==0);
        read_state(handles[1],XTY_READ_DATA);
        CHECK(Read(handles[1],b,sizeof b)==1 && b[0]=='x');
        read_state(handles[1],XTY_READ_WAIT);
        memset(&control,0,sizeof control);control.version=1;control.operation=XTY_SET_CONFIG;
        control.values[2]=XTY_CANON;control.values[3]=4;
        CHECK(aros_tty_control(handles[1],&control)==DOSTRUE);
        CHECK(Write(handles[0],"a",1)==1);
        read_state(handles[1],XTY_READ_WAIT);
        CHECK(Write(handles[0],"\n\004",2)==2);
        read_state(handles[1],XTY_READ_DATA);
        CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"a\n",2));
        /* Unlike WaitForChar, a readiness snapshot identifies a queued EOF
           without consuming it or claiming that payload bytes exist. */
        CHECK(WaitForChar(handles[1],0)==DOSFALSE);
        read_state(handles[1],XTY_READ_EOF);
        CHECK(Read(handles[1],b,sizeof b)==0);
        read_state(handles[1],XTY_READ_WAIT);
        CHECK(DoPkt(fh->fh_Type,XTY_ACTION_CONTROL,fh->fh_Arg1,1,64,0,0)==DOSFALSE);
        CHECK(IoErr()==ERROR_BAD_NUMBER);
        CHECK(DoPkt(((struct FileHandle *)BADDR(handles[0]))->fh_Type,
                    XTY_ACTION_CONTROL,fh->fh_Arg1,1,64,XTY_PACKET_MAGIC,0)==DOSFALSE);
        CHECK(IoErr()==ERROR_INVALID_LOCK);
        CHECK(DoPkt(fh->fh_Type,XTY_ACTION_CONTROL,fh->fh_Arg1,0,64,XTY_PACKET_MAGIC,0)==DOSFALSE);
        CHECK(IoErr()==ERROR_BAD_NUMBER);
        {
            XtyControl before;BPTR nil;
            memset(&control,0,sizeof control);control.version=99;control.operation=XTY_GET_CONFIG;
            before=control;
            CHECK(aros_tty_control(handles[1],&control)==DOSFALSE && IoErr()==ERROR_ACTION_NOT_KNOWN);
            CHECK(!memcmp(&control,&before,sizeof control));
            nil=Open((CONST_STRPTR)"NIL:",MODE_NEWFILE);CHECK(nil);
            control.version=1;before=control;
            CHECK(aros_tty_control(nil,&control)==DOSFALSE);
            CHECK(!memcmp(&control,&before,sizeof control));CHECK(Close(nil));
        }
        memset(&control,0,sizeof control);control.version=1;control.operation=XTY_SET_CONFIG;
        control.values[0]=1;control.values[1]=3;control.values[2]=3;control.values[3]=4;
        CHECK(aros_tty_control(handles[1],&control)==DOSTRUE);
        CHECK(Write(handles[0],"p\r",2)==2);
        CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"p\n",2));
        CHECK(Read(handles[0],b,sizeof b)==3 && !memcmp(b,"p\r\n",3));
        memset(&control,0,sizeof control);control.version=1;control.operation=XTY_SET_CONFIG;control.values[3]=4;
        CHECK(aros_tty_control(handles[1],&control)==DOSTRUE);
    }
    CHECK(aros_tty_worker_get_config(workers[0],1,&raw)==TTY_OK);
    CHECK(raw.local_flags==0 && raw.input_flags==0 && raw.output_flags==0);
    /* Native mode packets contain no handle ID: endpoint routing must not
       let the master change the application's line discipline. */
    CHECK(SetMode(handles[1],0)==DOSTRUE);
    CHECK(aros_tty_worker_get_config(workers[0],1,&got)==TTY_OK);
    CHECK(got.local_flags==(TTY_CANON|TTY_ECHO));
    CHECK(SetMode(handles[0],1)==DOSFALSE && IoErr()==ERROR_ACTION_NOT_KNOWN);
    CHECK(SetMode(handles[1],7)==DOSFALSE && IoErr()==ERROR_BAD_NUMBER);
    CHECK(Write(handles[0],"line",4)==4);
    CHECK(SetMode(handles[1],1)==DOSTRUE);
    CHECK(Read(handles[1],b,sizeof b)==4 && !memcmp(b,"line",4));
    CHECK(Write(handles[0],"\n",1)==1);
    CHECK(Read(handles[1],b,sizeof b)==1 && b[0]=='\n');
    CHECK(Read(handles[0],b,sizeof b)==4 && !memcmp(b,"line",4));
    CHECK(SetMode(handles[1],1)==DOSTRUE);
    CHECK(aros_tty_worker_get_config(workers[0],1,&got)==TTY_OK);
    CHECK(got.local_flags==0 && got.input_flags==0 && got.output_flags==0);
    CHECK(Write(handles[0],"\r\004",2)==2);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"\r\004",2));
    /* A valid slave ID on the master port must not write to master output. */
    packet->dp_Type=ACTION_WRITE;
    packet->dp_Arg1=((struct FileHandle *)BADDR(handles[1]))->fh_Arg1;
    packet->dp_Arg2=(SIPTR)"wrong";packet->dp_Arg3=5;
    pending=1;SendPkt(packet,((struct FileHandle *)BADDR(handles[0]))->fh_Type,reply);
    WaitPort(reply);CHECK(GetMsg(reply)==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==-1 && packet->dp_Res2==ERROR_INVALID_LOCK);
    packet->dp_Type=ACTION_END;
    pending=1;SendPkt(packet,((struct FileHandle *)BADDR(handles[0]))->fh_Type,reply);
    WaitPort(reply);CHECK(GetMsg(reply)==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==DOSFALSE && packet->dp_Res2==ERROR_INVALID_LOCK);
    /* Removing echo must wake an already admitted write without another DOS
       request incidentally pumping the owner. The master-port barrier proves
       the write was dispatched before the slave's mode packet. */
    for(path=0;path<2;++path) {
    c=raw;c.local_flags=TTY_ECHO;
    CHECK(aros_tty_worker_configure(workers[0],&c)==TTY_OK);
    memset(full,'f',sizeof full);
    CHECK(Write(handles[1],full,sizeof full)==sizeof full);
    packet->dp_Type=ACTION_WRITE;
    packet->dp_Arg1=((struct FileHandle *)BADDR(handles[0]))->fh_Arg1;
    packet->dp_Arg2=(SIPTR)"x";packet->dp_Arg3=1;
    port=((struct FileHandle *)BADDR(handles[0]))->fh_Type;
    pending=1;SendPkt(packet,port,reply);
    CHECK(DoPkt(port,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
    CHECK(!GetMsg(reply));
    if(!path) CHECK(SetMode(handles[1],1)==DOSTRUE);
    else {
        XtyControl control;
        memset(&control,0,sizeof control);
        control.version=XTY_CONTROL_VERSION;control.operation=XTY_SET_CONFIG;
        control.values[3]=4;
        CHECK(aros_tty_control(handles[1],&control)==DOSTRUE);
    }
    completed=NULL;
    for(tick=0;tick<100 && !(completed=GetMsg(reply));++tick) Delay(1);
    CHECK(completed==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==1 && packet->dp_Res2==0);
    CHECK(Read(handles[1],b,sizeof b)==1 && b[0]=='x');
    CHECK(Read(handles[0],full,sizeof full)==sizeof full);
    for(i=0;i<sizeof full;++i) CHECK(full[i]=='f');
    }
    puts("TTY CONTROL WAKE PASS: SetMode and protocol SET_CONFIG release blocked echo write");
    c=raw;c.local_flags=TTY_CANON|TTY_ECHO;c.input_flags=TTY_ICRNL;c.output_flags=TTY_OPOST|TTY_ONLCR;
    CHECK(aros_tty_worker_configure(workers[0],&c)==TTY_OK);
    CHECK(SetMode(handles[1],1)==DOSTRUE);
    CHECK(aros_tty_worker_get_config(workers[0],1,&got)==TTY_OK);
    CHECK(got.local_flags==0 && got.input_flags==TTY_ICRNL &&
          got.output_flags==(TTY_OPOST|TTY_ONLCR) && got.eof==4);
    CHECK(SetMode(handles[1],0)==DOSTRUE);
    CHECK(Write(handles[0],"a\r",2)==2);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"a\n",2));
    CHECK(Read(handles[0],b,sizeof b)==3 && !memcmp(b,"a\r\n",3));
    CHECK(Write(handles[0],"tail",4)==4);
    CHECK(aros_tty_worker_configure(workers[0],&raw)==TTY_OK);
    CHECK(Read(handles[1],b,sizeof b)==4 && !memcmp(b,"tail",4));
    CHECK(Read(handles[0],b,sizeof b)==4 && !memcmp(b,"tail",4));
    CHECK(Write(handles[0],"\004",1)==1);
    CHECK(Read(handles[1],b,sizeof b)==1 && b[0]==4);
    CHECK(aros_tty_worker_configure(workers[0],&raw)==TTY_OK);
    CHECK(aros_tty_worker_duplicate(workers[0],handles[1],&handles[15])==TTY_OK);
    packet->dp_Type=ACTION_READ;
    packet->dp_Arg1=((struct FileHandle *)BADDR(handles[1]))->fh_Arg1;
    packet->dp_Arg2=(SIPTR)b;packet->dp_Arg3=sizeof b;
    port=((struct FileHandle *)BADDR(handles[1]))->fh_Type;
    pending=1;SendPkt(packet,port,reply);
    CHECK(DoPkt(port,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
    CHECK(!GetMsg(reply));
    close_handle(&handles[1]);handles[1]=handles[15];handles[15]=BNULL;
    CHECK(GetMsg(reply)==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==-1 && packet->dp_Res2==ERROR_BREAK);
    CHECK(!GetMsg(reply)); /* Survivor below must remain usable, no duplicate reply. */
    CHECK(SetMode(handles[1],0)==DOSTRUE);
    CHECK(Write(handles[0],"s\n",2)==2);
    CHECK(Read(handles[1],b,sizeof b)==2 && !memcmp(b,"s\n",2));
    CHECK(Read(handles[0],b,sizeof b)==2 && !memcmp(b,"s\n",2));
    CHECK(SetMode(handles[1],1)==DOSTRUE);
    c=raw;c.local_flags=0x80000000u;
    CHECK(aros_tty_worker_configure(workers[0],&c)==TTY_UNSUPPORTED);
    CHECK(aros_tty_worker_get_config(workers[0],1,&got)==TTY_OK);
    CHECK(got.local_flags==raw.local_flags);
    got.eof=77;
    CHECK(aros_tty_worker_get_config(workers[0],99,&got)==TTY_UNSUPPORTED && got.eof==77);
    CHECK(aros_tty_worker_get_geometry(workers[0],1,&g)==TTY_OK);
    CHECK(g.rows==24 && g.columns==80);
    g.rows=31;g.columns=109;g.xpixel=872;g.ypixel=527;before=g;
    CHECK(aros_tty_worker_set_geometry(workers[0],&g)==TTY_OK);
    g.rows=0;CHECK(aros_tty_worker_set_geometry(workers[0],&g)==TTY_INVALID);
    CHECK(aros_tty_worker_get_geometry(workers[0],1,&g)==TTY_OK);
    CHECK(g.rows==before.rows && g.columns==before.columns && g.xpixel==872 && g.ypixel==527);
    g.rows=7;CHECK(aros_tty_worker_get_geometry(workers[0],99,&g)==TTY_UNSUPPORTED && g.rows==7);
    CHECK(aros_tty_worker_get_geometry(workers[1],1,&g)==TTY_OK);
    CHECK(g.rows==24 && g.columns==80);
    CHECK(aros_tty_worker_duplicate(workers[0],other_master,&unchanged)==TTY_INVALID);
    CHECK(unchanged==(BPTR)123);
    CHECK(aros_tty_worker_duplicate(workers[0],handles[0],&handles[2])==TTY_OK);
    stale=((struct FileHandle *)BADDR(handles[0]))->fh_Arg1;
    port=((struct FileHandle *)BADDR(handles[0]))->fh_Type;
    close_handle(&handles[0]);
    CHECK(Write(handles[2],"dup",3)==3);
    CHECK(Read(handles[1],b,sizeof b)==3 && !memcmp(b,"dup",3));
    /* Reuse the export slot, but never the retired handle ID. */
    CHECK(aros_tty_worker_duplicate(workers[0],handles[2],&handles[0])==TTY_OK);
    packet->dp_Type=ACTION_WRITE;packet->dp_Arg1=stale;packet->dp_Arg2=(SIPTR)"x";packet->dp_Arg3=1;
    pending=1;SendPkt(packet,port,reply);WaitPort(reply);
    CHECK(GetMsg(reply)==packet->dp_Link);pending=0;
    CHECK(packet->dp_Res1==-1 && packet->dp_Res2==ERROR_INVALID_LOCK);
    for(i=3;i<16;++i) CHECK(aros_tty_worker_duplicate(workers[0],handles[2],&handles[i])==TTY_OK);
    CHECK(aros_tty_worker_duplicate(workers[0],handles[2],&unchanged)==TTY_BUSY);
    CHECK(unchanged==(BPTR)123);
    CHECK(!aros_tty_worker_stop(workers[0]));
    CHECK(SetMode(handles[1],0)==DOSFALSE && IoErr()==ERROR_BREAK);
    CHECK(aros_tty_worker_duplicate(workers[0],handles[2],&unchanged)==TTY_CLOSED);
    CHECK(aros_tty_worker_set_geometry(workers[0],&before)==TTY_CLOSED);
    CHECK(aros_tty_worker_finish(workers[0])==-1);
    for(i=0;i<16;++i) close_handle(&handles[i]);
    CHECK(!aros_tty_worker_finish(workers[0]));workers[0]=NULL;
    close_handle(&other_master);close_handle(&other_slave);
    CHECK(!aros_tty_worker_finish(workers[1]));workers[1]=NULL;
    FreeDosObject(DOS_STDPKT,packet);DeleteMsgPort(reply);
    puts("TTY CONTROLS PASS: native SetMode, wrong-port rejection, mode-change wakeup, canonical/echo, preserved input, geometry isolation, master/slave duplicates, stale IDs, 16 exports, stop/Close/reap");
    return 0;
}
