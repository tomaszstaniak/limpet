#include "../src/aros_tty_packets.h"
#include "../src/tty_control.h"
#include "../src/console_geometry.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyPackets endpoint;
static struct MsgPort *reply;
#define PACKET_COUNT (TTY_SERVICE_REQUESTS+1)
static struct DosPacket *packets[PACKET_COUNT];
static struct FileHandle *master,*slave;
static ULONG child_id;
static BPTR child_fh;
static int child_ok;

static void fail(const char *expr,int line)
{
    unsigned i;
    printf("TTY DOS FAIL line=%d: %s\n",line,expr);fflush(stdout);
    /* Keep shared code/storage alive until our client has actually exited. */
    aros_tty_packets_stop(&endpoint);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) {
        aros_tty_packets_poll(&endpoint);Delay(1);
    }
    if(child_id) { ChildFree(child_id);child_id=0; }
    aros_tty_packets_poll(&endpoint);
    if(reply) while(GetMsg(reply)) {}
    /* All clients are now quiescent. Release remaining exported ownership
       through real END packets, even though stop already hung up the core. */
    if(endpoint.ports[TTY_MASTER] && packets[0] && reply) for(i=0;i<TTY_SERVICE_HANDLES;++i) {
        if(endpoint.exports[i]) {
            packets[0]->dp_Type=ACTION_END;packets[0]->dp_Arg1=(SIPTR)endpoint.exports[i];
            SendPkt(packets[0],endpoint.ports[endpoint.export_ends[i]],reply);
            aros_tty_packets_poll(&endpoint);
            while(GetMsg(reply)) {}
        }
    }
    if(aros_tty_packets_dispose(&endpoint)) {
        puts("TTY DOS unresolved ownership: retaining process for safety");fflush(stdout);
        for(;;) { aros_tty_packets_poll(&endpoint);Delay(50); }
    }
    if(master) FreeDosObject(DOS_FILEHANDLE,master);
    if(slave) FreeDosObject(DOS_FILEHANDLE,slave);
    for(i=0;i<PACKET_COUNT;++i) if(packets[i]) FreeDosObject(DOS_STDPKT,packets[i]);
    if(reply) DeleteMsgPort(reply);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)

static void submit(unsigned slot,LONG action,SIPTR handle,void *buffer,SIPTR n)
{
    struct DosPacket *p=packets[slot];
    p->dp_Type=action;p->dp_Arg1=handle;p->dp_Arg2=(SIPTR)buffer;p->dp_Arg3=n;
    /* This fixture uses only the original master=1/slave=2 exports. */
    SendPkt(p,endpoint.ports[handle==2?TTY_SLAVE:TTY_MASTER],reply);
}
static void receive(unsigned slot,SIPTR value,LONG error)
{
    unsigned tick;
    for(tick=0;tick<500;++tick) {
        struct Message *m;
        aros_tty_packets_poll(&endpoint);
        m=GetMsg(reply);
        if(m) {
            CHECK(m==packets[slot]->dp_Link);
            CHECK(packets[slot]->dp_Res1==value);
            CHECK(packets[slot]->dp_Res2==error);
            return;
        }
        Delay(1);
    }
    fail("reply deadline",__LINE__);
}
static void native_client(void)
{
    unsigned char b[4];
    Wait(SIGBREAKF_CTRL_F);
    child_ok=Read(child_fh,b,sizeof b)==4 && !memcmp(b,"ping",4) &&
             Write(child_fh,"pong",4)==4;
    if(!Close(child_fh)) child_ok=0;
}
static void zero_length_behind_blocked(void)
{
    unsigned lane;unsigned char fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY],byte=0;
    memset(fill,'Z',sizeof fill);
    for(lane=0;lane<4;++lane) {
        SIPTR id=lane/2?slave->fh_Arg1:master->fh_Arg1;
        SIPTR peer=lane/2?master->fh_Arg1:slave->fh_Arg1;
        LONG action=lane%2?ACTION_WRITE:ACTION_READ;
        if(action==ACTION_WRITE) {
            submit(0,action,id,fill,sizeof fill);receive(0,sizeof fill,0);
        }
        submit(0,action,id,&byte,1);
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        submit(1,action,id,NULL,0);receive(1,0,0);
        CHECK(!GetMsg(reply));
        CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
        receive(0,-1,ERROR_BREAK);CHECK(!GetMsg(reply));
        if(action==ACTION_WRITE) {
            submit(0,ACTION_READ,peer,out,sizeof out);receive(0,sizeof out,0);
            CHECK(!memcmp(fill,out,sizeof out));
        }
    }
}
static void control_submit(unsigned slot,SIPTR id,XtyControl *c)
{
    packets[slot]->dp_Arg4=XTY_PACKET_MAGIC;packets[slot]->dp_Arg5=0;
    submit(slot,XTY_ACTION_CONTROL,id,c,sizeof *c);
}
/* Real packet ownership: no premature/duplicate reply, unchanged payload
   until success, cancellation/stop cannot apply a deferred mode change. */
static void drain_packets(void)
{
    XtyControl c,before;unsigned op,i;unsigned char b[8];TtyConfig raw;
    CHECK(tty_pair_get_config(&endpoint.service.pair,1,&raw)==TTY_OK);
    for(op=9;op<=11;++op) {
        CHECK(tty_pair_configure(&endpoint.service.pair,&raw)==TTY_OK);
        submit(0,ACTION_WRITE,2,(void *)"old",3);receive(0,3,0);
        memset(&c,0,sizeof c);c.version=1;c.operation=op;
        if(op!=9) { c.values[1]=3;c.values[3]=4; }
        before=c;control_submit(0,2,&c);
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        CHECK(!memcmp(&c,&before,sizeof c));
        submit(1,ACTION_WRITE,2,(void *)"\n",1);
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        if(op==11) { submit(2,ACTION_WRITE,1,(void *)"stale",5);receive(2,5,0); }
        submit(2,ACTION_READ,1,b,2);receive(2,2,0);CHECK(!memcmp(b,"ol",2));
        CHECK(!GetMsg(reply));CHECK(!memcmp(&c,&before,sizeof c));
        submit(2,ACTION_READ,1,b,1);
        receive(0,DOSTRUE,0);receive(1,1,0);receive(2,1,0);CHECK(b[0]=='d');
        CHECK(c.reserved[0]==XTY_PACKET_MAGIC && c.operation==op);
        CHECK(c.values[1]==(op==9?0:3));
        for(i=4;i<12;++i) CHECK(!c.values[i]);
        CHECK(!endpoint.service.pair.input.count);
        submit(0,ACTION_READ,1,b,sizeof b);receive(0,op==9?1:2,0);
        CHECK(!memcmp(b,op==9?"\n":"\r\n",op==9?1:2));CHECK(!GetMsg(reply));
        memset(&c,0,sizeof c);c.version=1;c.operation=op;
        if(op!=9) c.values[3]=4;
        control_submit(0,1,&c);receive(0,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
    }
    CHECK(tty_pair_configure(&endpoint.service.pair,&raw)==TTY_OK);
    submit(0,ACTION_WRITE,2,(void *)"x",1);receive(0,1,0);
    memset(&c,0,sizeof c);c.version=1;c.operation=11;c.values[1]=3;c.values[3]=4;
    before=c;control_submit(0,2,&c);aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
    receive(0,DOSFALSE,ERROR_BREAK);CHECK(!memcmp(&c,&before,sizeof c));
    CHECK(!endpoint.service.pair.config.output_flags);CHECK(!GetMsg(reply));
    submit(0,ACTION_READ,1,b,1);receive(0,1,0);CHECK(b[0]=='x');
    puts("TTY DRAIN PACKETS PASS: three operations, partial reads, ordered writes, payload lifetime, cancellation");fflush(stdout);
}
static void drain_pressure_lifetime(void)
{
    XtyControl controls[8],before;unsigned i,event;unsigned char b;TtyConfig raw;
    CHECK(tty_pair_get_config(&endpoint.service.pair,1,&raw)==TTY_OK);
    submit(0,ACTION_WRITE,2,(void *)"x",1);receive(0,1,0);
    for(i=0;i<8;++i) {
        memset(&controls[i],0,sizeof controls[i]);controls[i].version=1;controls[i].operation=9;
        control_submit(i,2,&controls[i]);
    }
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    submit(8,ACTION_WRITE,2,(void *)"z",1);receive(8,-1,ERROR_OBJECT_IN_USE);
    submit(9,ACTION_READ,1,&b,1);
    for(i=0;i<8;++i) receive(i,DOSTRUE,0);
    receive(9,1,0);CHECK(b=='x' && !GetMsg(reply));
    for(event=0;event<3;++event) {
        submit(0,ACTION_WRITE,1,(void *)"keep",4);receive(0,4,0);
        submit(0,ACTION_WRITE,2,(void *)"x",1);receive(0,1,0);
        memset(&controls[0],0,sizeof controls[0]);controls[0].version=1;
        controls[0].operation=11;controls[0].values[1]=3;controls[0].values[3]=4;
        before=controls[0];control_submit(0,2,&controls[0]);
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        if(event<2) {
            submit(1,ACTION_END,event==0?2:1,NULL,0);
            receive(0,DOSFALSE,event==0?ERROR_BREAK:ERROR_INVALID_LOCK);receive(1,DOSTRUE,0);
        } else {
            aros_tty_packets_stop(&endpoint);receive(0,DOSFALSE,ERROR_BREAK);
        }
        CHECK(!memcmp(&controls[0],&before,sizeof before));CHECK(!GetMsg(reply));
        CHECK(!endpoint.service.pair.config.output_flags);
        CHECK(endpoint.service.pair.input.count==4);
        for(i=0;i<2;++i) if(endpoint.exports[i]) {
            submit(1,ACTION_END,(SIPTR)endpoint.exports[i],NULL,0);receive(1,DOSTRUE,0);
        }
        CHECK(!aros_tty_packets_dispose(&endpoint));
        FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);
        master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
        CHECK(master && slave);CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    }
    puts("TTY DRAIN PACKET LIFETIME PASS: eight-slot pressure, reader progress, own/peer close, stop, exact-once reply, disposal");fflush(stdout);
}
static void cancel_packets(void)
{
    unsigned char out=0xa5;unsigned cycle;
    for(cycle=0;cycle<100;++cycle) {
        submit(0,ACTION_READ,2,&out,1);
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        packets[1]->dp_Arg4=0x58545931L;packets[1]->dp_Arg5=0;
        submit(1,0x58545902L,2,packets[0],0);
        receive(0,-1,ERROR_BREAK);receive(1,DOSTRUE,0);
        CHECK(out==0xa5 && !GetMsg(reply));
        /* A repeated request must not touch already returned packet storage. */
        submit(1,0x58545902L,2,packets[0],0);
        receive(1,DOSFALSE,ERROR_OBJECT_NOT_FOUND);
    }
    puts("TTY CANCEL PACKETS PASS: cycles=100 pending read, unchanged buffer, exact replies, completed target");fflush(stdout);
    submit(0,ACTION_READ,2,&out,1);aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    packets[1]->dp_Arg4=0;submit(1,0x58545902L,2,packets[0],0);
    receive(1,DOSFALSE,ERROR_BAD_NUMBER);CHECK(!GetMsg(reply));
    packets[1]->dp_Arg4=0x58545931L;submit(1,0x58545902L,1,packets[0],0);
    receive(1,DOSFALSE,ERROR_OBJECT_NOT_FOUND);CHECK(!GetMsg(reply));
    submit(1,0x58545902L,2,(void *)(IPTR)1,0);
    receive(1,DOSFALSE,ERROR_OBJECT_NOT_FOUND);CHECK(!GetMsg(reply));
    {
        struct MsgPort *other=CreateMsgPort();CHECK(other);
        packets[1]->dp_Arg2=(SIPTR)packets[0];
        SendPkt(packets[1],endpoint.ports[TTY_SLAVE],other);aros_tty_packets_poll(&endpoint);
        CHECK(GetMsg(other)==packets[1]->dp_Link);
        CHECK(packets[1]->dp_Res1==DOSFALSE && packets[1]->dp_Res2==ERROR_OBJECT_NOT_FOUND);
        DeleteMsgPort(other);CHECK(!GetMsg(reply));
    }
    submit(1,0x58545902L,2,packets[0],0);
    receive(0,-1,ERROR_BREAK);receive(1,DOSTRUE,0);CHECK(out==0xa5);
    puts("TTY CANCEL VALIDATION PASS: magic, export, arbitrary address, foreign reply port, retained read");fflush(stdout);
    {
        unsigned i;unsigned char blocked[TTY_SERVICE_REQUESTS],fill[TTY_QUEUE_CAPACITY];
        TtyConfig raw=endpoint.service.pair.config,canon=raw;
        CHECK(TTY_SERVICE_REQUESTS<PACKET_COUNT);memset(blocked,0xa5,sizeof blocked);
        memset(fill,'x',sizeof fill);canon.local_flags=TTY_CANON;
        CHECK(tty_pair_configure(&endpoint.service.pair,&canon)==TTY_OK);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,fill,TTY_LINE_CAPACITY).count==TTY_LINE_CAPACITY);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,fill,TTY_QUEUE_CAPACITY).count==TTY_QUEUE_CAPACITY);
        CHECK(tty_pair_output_flow(&endpoint.service.pair,1)==TTY_OK);
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            unsigned lane=i/TTY_SERVICE_LANE_REQUESTS;
            submit(i,lane%2?ACTION_WRITE:ACTION_READ,lane/2?2:1,&blocked[i],1);
        }
        aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
        packets[TTY_SERVICE_REQUESTS]->dp_Arg4=0x58545931L;
        packets[TTY_SERVICE_REQUESTS]->dp_Arg5=0;
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            submit(TTY_SERVICE_REQUESTS,0x58545902L,packets[i]->dp_Arg1,packets[i],0);
            receive(i,-1,ERROR_BREAK);receive(TTY_SERVICE_REQUESTS,DOSTRUE,0);
            CHECK(blocked[i]==0xa5);
        }
        CHECK(!GetMsg(reply));
        CHECK(tty_pair_flush(&endpoint.service.pair,TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT)==TTY_OK);
        CHECK(tty_pair_output_flow(&endpoint.service.pair,0)==TTY_OK);
        CHECK(tty_pair_configure(&endpoint.service.pair,&raw)==TTY_OK);
    }
    puts("TTY CANCEL PRESSURE PASS: all service slots occupied, out-of-band cancellation, unchanged buffers");fflush(stdout);
}
static void output_flow_packets(void)
{
    XtyControl flow,drain;unsigned char out[4];
    memset(&flow,0,sizeof flow);flow.version=1;flow.operation=12;flow.values[0]=1;
    control_submit(0,2,&flow);receive(0,DOSTRUE,0);
    submit(0,ACTION_WRITE,2,(void *)"held",4);receive(0,4,0);
    submit(0,ACTION_READ,1,out,sizeof out);
    memset(&drain,0,sizeof drain);drain.version=1;drain.operation=9;
    control_submit(1,2,&drain);aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    memset(&flow,0,sizeof flow);flow.version=1;flow.operation=12;flow.values[0]=2;
    control_submit(2,2,&flow);
    receive(0,4,0);receive(1,DOSTRUE,0);receive(2,DOSTRUE,0);
    CHECK(!memcmp(out,"held",4));CHECK(!GetMsg(reply));
    puts("TTY OUTPUT FLOW PACKETS PASS: resume bypasses held drain, pending read wakes, exact bytes");fflush(stdout);
}
static void read_timer_cancel(void)
{
    unsigned char a[4],b[4];TtyConfig c=endpoint.service.pair.config;
    c.vmin=0;c.vtime=1;CHECK(tty_pair_configure(&endpoint.service.pair,&c)==TTY_OK);
    submit(0,ACTION_READ,2,a,sizeof a);submit(1,ACTION_READ,2,b,sizeof b);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
    CHECK(GetMsg(reply)==packets[0]->dp_Link && packets[0]->dp_Res2==ERROR_BREAK);
    /* No poll: cancellation itself must arm the successor's physical timer. */
    Delay(10);CHECK(endpoint.timed_read && CheckIO((struct IORequest *)endpoint.read_timer));
    receive(1,0,0);CHECK(!GetMsg(reply));
    c.vmin=1;c.vtime=0;CHECK(tty_pair_configure(&endpoint.service.pair,&c)==TTY_OK);
    puts("TTY READ TIMER CANCEL PASS: successor expires without incidental poll");fflush(stdout);
}
static void console_geometry_packets(void)
{
    struct ConsoleGeometry geometry,before;
    TtyGeometry saved=endpoint.service.pair.geometry;
    TtyGeometry grid={TTY_PAIR_VERSION,37,101,909,703};
    unsigned id;
    memset(&geometry,0xa5,sizeof geometry);before=geometry;
    packets[0]->dp_Arg4=CONSOLE_GEOMETRY_VERSION;packets[0]->dp_Arg5=0;
    submit(0,ACTION_CONSOLE_GETGEOMETRY,2,&geometry,sizeof geometry);
    receive(0,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
    CHECK(!memcmp(&geometry,&before,sizeof geometry));
    endpoint.presentation_enabled=1;
    CHECK(tty_pair_set_geometry(&endpoint.service.pair,&grid)==TTY_OK);
    for(id=1;id<=2;++id) {
        submit(0,ACTION_CONSOLE_GETGEOMETRY,id,&geometry,sizeof geometry);
        receive(0,DOSTRUE,0);
        CHECK(geometry.cg_Size==24 && geometry.cg_Version==1);
        CHECK(geometry.cg_Columns==101 && geometry.cg_Rows==37);
        CHECK(geometry.cg_Width==909 && geometry.cg_Height==703);
    }
    before=geometry;
    submit(0,ACTION_CONSOLE_GETGEOMETRY,0,&geometry,sizeof geometry);
    receive(0,DOSFALSE,ERROR_INVALID_LOCK);
    CHECK(!memcmp(&geometry,&before,sizeof geometry));
    submit(0,ACTION_CONSOLE_GETGEOMETRY,2,&geometry,sizeof geometry-1);
    receive(0,DOSFALSE,ERROR_BAD_NUMBER);
    CHECK(!memcmp(&geometry,&before,sizeof geometry));
    packets[0]->dp_Arg4=2;
    submit(0,ACTION_CONSOLE_GETGEOMETRY,2,&geometry,sizeof geometry);
    receive(0,DOSFALSE,ERROR_BAD_NUMBER);
    CHECK(!memcmp(&geometry,&before,sizeof geometry));
    packets[0]->dp_Arg4=1;endpoint.stopping=1;
    submit(0,ACTION_CONSOLE_GETGEOMETRY,2,&geometry,sizeof geometry);
    receive(0,DOSFALSE,ERROR_INVALID_LOCK);
    CHECK(!memcmp(&geometry,&before,sizeof geometry));
    endpoint.stopping=0;endpoint.presentation_enabled=0;
    packets[0]->dp_Arg4=0;
    CHECK(tty_pair_set_geometry(&endpoint.service.pair,&saved)==TTY_OK);
    puts("TTY CONSOLE GEOMETRY PASS: copied snapshot, both ends, renderer, lifetime, arguments");
    fflush(stdout);
}
int main(void)
{
    unsigned i,tick;unsigned char b[16],line[TTY_LINE_CAPACITY+1];
    struct Process *child;TtyConfig raw,canon;
    reply=CreateMsgPort();CHECK(reply);
    for(i=0;i<PACKET_COUNT;++i) { packets[i]=AllocDosObject(DOS_STDPKT,NULL);CHECK(packets[i]); }
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(master && slave);
    CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    CHECK(aros_tty_packets_dispose(&endpoint)==-1);

    console_geometry_packets();
    cancel_packets();output_flow_packets();read_timer_cancel();drain_packets();drain_pressure_lifetime();zero_length_behind_blocked();

    submit(0,ACTION_READ,slave->fh_Arg1,b,3);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    submit(1,ACTION_WRITE,master->fh_Arg1,(void *)"a\0b",3);
    receive(0,3,0);receive(1,3,0);CHECK(!memcmp(b,"a\0b",3));
    submit(0,ACTION_READ,99,b,1);receive(0,-1,ERROR_INVALID_LOCK);
    submit(0,ACTION_READ,slave->fh_Arg1,b,-1);receive(0,-1,ERROR_BAD_NUMBER);
    submit(0,ACTION_READ,slave->fh_Arg1,b,(SIPTR)INT32_MAX+1);receive(0,-1,ERROR_BAD_NUMBER);
    submit(0,ACTION_WRITE,master->fh_Arg1,NULL,1);receive(0,-1,ERROR_BAD_NUMBER);
    submit(0,ACTION_READ,slave->fh_Arg1,NULL,0);receive(0,0,0);
    submit(0,ACTION_SEEK,master->fh_Arg1,NULL,0);receive(0,-1,ERROR_ACTION_NOT_KNOWN);
    submit(0,ACTION_READ,slave->fh_Arg1,b,1);aros_tty_packets_poll(&endpoint);
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
    receive(0,-1,ERROR_BREAK);CHECK(!GetMsg(reply));

    packets[0]->dp_Type=ACTION_WAIT_CHAR;packets[0]->dp_Arg1=5000000;
    SendPkt(packets[0],endpoint.ports[TTY_SLAVE],reply);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    /* Empty reads cannot steal a waiter's input. Invalid I/O must report
       its malformed envelope consistently, even while that waiter exists. */
    submit(1,ACTION_READ,slave->fh_Arg1,NULL,0);receive(1,0,0);
    CHECK(endpoint.waits[TTY_SLAVE].packet==packets[0]);
    submit(1,ACTION_READ,slave->fh_Arg1,b,-1);receive(1,-1,ERROR_BAD_NUMBER);
    submit(1,ACTION_READ,slave->fh_Arg1,b,(SIPTR)INT32_MAX+1);
    receive(1,-1,ERROR_BAD_NUMBER);
    submit(1,ACTION_READ,slave->fh_Arg1,NULL,1);receive(1,-1,ERROR_BAD_NUMBER);
    submit(1,ACTION_READ,slave->fh_Arg1,b,1);receive(1,-1,ERROR_OBJECT_IN_USE);
    CHECK(endpoint.waits[TTY_SLAVE].packet==packets[0]);
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
    receive(0,DOSFALSE,ERROR_BREAK);CHECK(!GetMsg(reply));
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_INVALID);

    CHECK(tty_pair_get_config(&endpoint.service.pair,1,&raw)==TTY_OK);
    canon=raw;canon.local_flags=TTY_CANON;
    CHECK(tty_pair_configure(&endpoint.service.pair,&canon)==TTY_OK);
    memset(line,'a',TTY_LINE_CAPACITY);
    submit(0,ACTION_WRITE,master->fh_Arg1,line,TTY_LINE_CAPACITY);
    receive(0,TTY_LINE_CAPACITY,0);
    submit(0,ACTION_WRITE,master->fh_Arg1,(void *)"x",1);
    submit(1,ACTION_WRITE,master->fh_Arg1,(void *)"\n",1);
    packets[2]->dp_Type=ACTION_WAIT_CHAR;packets[2]->dp_Arg1=5000000;
    SendPkt(packets[2],endpoint.ports[TTY_SLAVE],reply);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    CHECK(aros_tty_packets_cancel(&endpoint,packets[0])==TTY_OK);
    /* No poll here: cancellation itself must finish the now-ready waiter. */
    CHECK(GetMsg(reply)==packets[0]->dp_Link);
    CHECK(packets[0]->dp_Res1==-1 && packets[0]->dp_Res2==ERROR_BREAK);
    CHECK(GetMsg(reply)==packets[1]->dp_Link);
    CHECK(packets[1]->dp_Res1==1 && packets[1]->dp_Res2==0);
    CHECK(GetMsg(reply)==packets[2]->dp_Link);
    CHECK(packets[2]->dp_Res1==DOSTRUE && packets[2]->dp_Res2==0);
    submit(0,ACTION_READ,slave->fh_Arg1,line,sizeof line);receive(0,sizeof line,0);
    for(i=0;i<TTY_LINE_CAPACITY;++i) CHECK(line[i]=='a');
    CHECK(line[TTY_LINE_CAPACITY]=='\n');
    CHECK(tty_pair_configure(&endpoint.service.pair,&raw)==TTY_OK);

    child_fh=MKBADDR(slave);
    child=CreateNewProcTags(NP_Entry,(IPTR)native_client,NP_Name,(IPTR)"TTY packet client",
        NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
    child_id=GetETaskID(&child->pr_Task);slave=NULL; /* ownership transferred */
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    submit(0,ACTION_WRITE,master->fh_Arg1,(void *)"ping",4);receive(0,4,0);
    submit(0,ACTION_READ,master->fh_Arg1,b,4);receive(0,4,0);CHECK(!memcmp(b,"pong",4));
    for(tick=0;tick<500 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
        aros_tty_packets_poll(&endpoint);Delay(1);
    }
    CHECK(ChildStatus(child_id)==CHILD_EXITED);
    ChildFree(child_id);child_id=0;CHECK(child_ok);
    puts("TTY DOS CHILD PASS: native Read/Write/Close and reaped child");
    submit(0,ACTION_READ,master->fh_Arg1,b,1);receive(0,0,0);
    submit(0,ACTION_END,master->fh_Arg1,NULL,0);receive(0,DOSTRUE,0);
    FreeDosObject(DOS_FILEHANDLE,master);master=NULL;
    CHECK(!aros_tty_packets_dispose(&endpoint));

    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(master && slave);
    CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    submit(0,ACTION_READ,master->fh_Arg1,b,1);
    submit(1,ACTION_READ,slave->fh_Arg1,b+1,1);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    aros_tty_packets_stop(&endpoint);
    receive(0,-1,ERROR_BREAK);receive(1,-1,ERROR_BREAK);
    CHECK(aros_tty_packets_dispose(&endpoint)==-1); /* live exported FileHandles */
    submit(0,ACTION_READ,1,b,1);receive(0,-1,ERROR_BREAK);
    submit(0,ACTION_END,master->fh_Arg1,NULL,0);receive(0,DOSTRUE,0);
    submit(0,ACTION_END,slave->fh_Arg1,NULL,0);receive(0,DOSTRUE,0);
    submit(0,ACTION_IS_FILESYSTEM,0,NULL,0);
    CHECK(aros_tty_packets_dispose(&endpoint)==-1); /* queued packet still owned */
    receive(0,DOSFALSE,0);
    CHECK(!aros_tty_packets_dispose(&endpoint));
    FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);
    master=slave=NULL;
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(master && slave);
    CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    submit(0,ACTION_READ,slave->fh_Arg1,b,1);
    aros_tty_packets_poll(&endpoint);CHECK(!GetMsg(reply));
    submit(1,ACTION_END,slave->fh_Arg1,NULL,0);
    receive(0,-1,ERROR_BREAK);receive(1,DOSTRUE,0);
    submit(0,ACTION_READ,master->fh_Arg1,b,1);receive(0,0,0);
    submit(0,ACTION_END,master->fh_Arg1,NULL,0);receive(0,DOSTRUE,0);
    CHECK(!aros_tty_packets_dispose(&endpoint));
    FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);
    master=slave=NULL;
    for(i=0;i<PACKET_COUNT;++i) FreeDosObject(DOS_STDPKT,packets[i]);
    DeleteMsgPort(reply);
    puts("TTY DOS PASS: real packets, binary duplex, pending/cancel, validation, native child Read/Write/Close, EOF, stop, disposal");
    return 0;
}
