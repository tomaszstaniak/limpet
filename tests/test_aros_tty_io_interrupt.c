#include "../src/aros_tty_packets.h"
#include "../src/aros_tty_client.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ArosTtyPackets endpoint;
static struct FileHandle *master,*slave,*extra;
static ULONG child_id;
static unsigned operation,phase;
static int try_mode;
static BPTR io_handle;
static unsigned char io_buffer[8];
static ULONG remaining_signal;
static int result,error;
static void cleanup(void)
{
    unsigned i;
    struct MsgPort *reply=CreateMsgPort();
    struct DosPacket *p=AllocDosObject(DOS_STDPKT,NULL);
    aros_tty_packets_stop(&endpoint);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) {
        aros_tty_packets_poll(&endpoint);Delay(1);
    }
    if(child_id) { ChildFree(child_id);child_id=0; }
    if(!reply || !p) for(;;) Delay(50);
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(endpoint.exports[i]) {
        p->dp_Type=ACTION_END;p->dp_Arg1=endpoint.exports[i];
        SendPkt(p,endpoint.ports[endpoint.export_ends[i]],reply);
        aros_tty_packets_poll(&endpoint);WaitPort(reply);GetMsg(reply);
    }
    if(aros_tty_packets_dispose(&endpoint)) for(;;) Delay(50);
    FreeDosObject(DOS_STDPKT,p);DeleteMsgPort(reply);
    if(master) FreeDosObject(DOS_FILEHANDLE,master);
    if(slave) FreeDosObject(DOS_FILEHANDLE,slave);
    if(extra) FreeDosObject(DOS_FILEHANDLE,extra);
    master=NULL;slave=NULL;extra=NULL;
}
static void fail(const char *expr,int line)
{
    printf("TTY IO INTERRUPT FAIL line=%d: %s\n",line,expr);fflush(stdout);
    cleanup();exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void client(void)
{
    if(phase==2 || phase==3) Signal(FindTask(NULL),SIGBREAKF_CTRL_C);
    if(try_mode)
        result=operation%2?aros_tty_write_try(io_handle,io_buffer,8):aros_tty_read_try(io_handle,io_buffer,8);
    else result=operation%2?aros_tty_write_break(io_handle,phase==3?NULL:io_buffer,phase==3?0:8):
        aros_tty_read_break(io_handle,phase==3?NULL:io_buffer,phase==3?0:8);
    error=IoErr();
    remaining_signal=SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C;
}
/* A deliberately faulty responder exercises reply validation without ever
   writing beyond the caller buffer. Normal tests below use the real adapter. */
static void response_cases(void)
{
    static const SIPTR counts[]={-2,9,((SIPTR)1<<32),3,0};
    unsigned direction,test,tick;
    for(direction=0;direction<2;++direction) for(test=0;test<8;++test) {
        struct Process *child;unsigned transfers=0,caps=0;
        operation=direction;phase=4;io_handle=MKBADDR(master);result=-99;error=0;
        memcpy(io_buffer,"ABCDEFGH",8);
        child=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"TTY reply validation",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        child_id=GetETaskID(&child->pr_Task);CHECK(child_id);
        for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            struct Message *m;
            while((m=GetMsg(endpoint.ports[TTY_MASTER]))) {
                struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
                if(p->dp_Type==XTY_ACTION_CONTROL) {
                    XtyControl *c=(XtyControl *)p->dp_Arg2;
                    memset(c->values,0,sizeof c->values);
                    c->values[0]=test==6?(try_mode?XTY_CAP_CANCEL:0):
                        test==7?XTY_CAP_TRY_IO:XTY_CAP_CANCEL|XTY_CAP_TRY_IO;
                    c->reserved[0]=XTY_PACKET_MAGIC;++caps;ReplyPkt(p,DOSTRUE,0);
                } else {
                    CHECK(p->dp_Type==(try_mode?(direction?XTY_ACTION_TRY_WRITE:XTY_ACTION_TRY_READ):
                                              (direction?ACTION_WRITE:ACTION_READ)));
                    if(try_mode) CHECK(p->dp_Arg4==XTY_PACKET_MAGIC && !p->dp_Arg5);
                    ++transfers;
                    if(test==5) {
                        /* ReplyPkt takes LONG for error, but the packet field
                           is SIPTR: emulate a broken raw packet responder. */
                        struct MsgPort *reply=p->dp_Port;
                        p->dp_Res1=3;p->dp_Res2=(SIPTR)1<<32;
                        p->dp_Port=endpoint.ports[TTY_MASTER];PutMsg(reply,p->dp_Link);
                    } else ReplyPkt(p,counts[test<5?test:0],test==3 || test==4?ERROR_BREAK:0);
                }
            }
            Delay(1);
        }
        CHECK(ChildStatus(child_id)==CHILD_EXITED);ChildFree(child_id);child_id=0;
        CHECK(caps==1 && transfers==(test>=6?0u:1u));
        CHECK(result==-1 && error==(test>=6?ERROR_ACTION_NOT_KNOWN:ERROR_BAD_NUMBER));
        CHECK(!memcmp(io_buffer,"ABCDEFGH",8));
    }
    printf("TTY IO REPLY PASS: try=%d directions=2 malformed_counts=6 contradictory_results=4 wide_errors=2 missing_capability=4 unchanged buffers\n",try_mode);fflush(stdout);
}
static void close_export(struct FileHandle *fh)
{
    struct MsgPort *reply=CreateMsgPort();struct DosPacket *p=AllocDosObject(DOS_STDPKT,NULL);
    struct Message *m;LONG value,error;
    CHECK(reply && p);p->dp_Type=ACTION_END;p->dp_Arg1=fh->fh_Arg1;
    SendPkt(p,fh->fh_Type,reply);aros_tty_packets_poll(&endpoint);m=GetMsg(reply);
    CHECK(m==p->dp_Link);value=p->dp_Res1;error=p->dp_Res2;
    FreeDosObject(DOS_STDPKT,p);DeleteMsgPort(reply);CHECK(value==DOSTRUE && !error);
}
static void run_normal(struct FileHandle *close_when_pending)
{
    struct Process *child;unsigned tick,i,admitted=0;
    result=-99;error=0;remaining_signal=0;memcpy(io_buffer,"ABCDEFGH",8);phase=4;
    child=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"TTY close client",
        NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
    child_id=GetETaskID(&child->pr_Task);CHECK(child_id);
    for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
        aros_tty_packets_poll(&endpoint);
        if(close_when_pending && !admitted) for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            if(endpoint.slots[i].packet) { ++admitted;close_export(close_when_pending);break; }
        }
        Delay(1);
    }
    CHECK(ChildStatus(child_id)==CHILD_EXITED);ChildFree(child_id);child_id=0;
    CHECK(admitted==(close_when_pending?1u:0u));
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) CHECK(!endpoint.slots[i].packet);
}
static void peer_close_cases(void)
{
    unsigned end,scenario;
    for(end=0;end<2;++end) for(scenario=0;scenario<2;++scenario) {
        struct FileHandle *peer;TtyEnd peer_end=end==TTY_MASTER?TTY_SLAVE:TTY_MASTER;
        cleanup();master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
        CHECK(master && slave);CHECK(!aros_tty_packets_init(&endpoint,master,slave));
        peer=end==TTY_MASTER?slave:master;io_handle=MKBADDR(end==TTY_MASTER?master:slave);
        operation=end*2;
        if(!scenario) {
            CHECK(tty_pair_write(&endpoint.service.pair,peer_end,"tail",4).count==4);
            if(end==TTY_MASTER) CHECK(tty_pair_output_flow(&endpoint.service.pair,1)==TTY_OK);
            close_export(peer);run_normal(NULL);
            CHECK(result==4 && !error && !memcmp(io_buffer,"tailEFGH",8));
            run_normal(NULL);CHECK(result==0 && !error && !memcmp(io_buffer,"ABCDEFGH",8));
        } else {
            extra=AllocDosObject(DOS_FILEHANDLE,NULL);CHECK(extra);
            CHECK(aros_tty_packets_duplicate(&endpoint,(TtyHandle)peer->fh_Arg1,extra)==TTY_OK);
            close_export(peer);run_normal(extra);
            CHECK(result==0 && !error && !memcmp(io_buffer,"ABCDEFGH",8));
        }
        ++operation;run_normal(NULL);
        CHECK(result==-1 && error==ERROR_INVALID_LOCK && !memcmp(io_buffer,"ABCDEFGH",8));
    }
    puts("TTY IO CLOSE PASS: ends=2 retained tail before EOF, paused output hangup, duplicate keeps read pending, final close wakes EOF, broken writes");fflush(stdout);
}
int main(void)
{
    unsigned cycle,tick,i;unsigned char fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(master && slave);CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    response_cases();
    try_mode=1;response_cases();try_mode=0;
    CHECK(aros_tty_read_break(BNULL,out,1)==-1 && IoErr()==ERROR_INVALID_LOCK);
    CHECK(aros_tty_write_break(BNULL,out,1)==-1 && IoErr()==ERROR_INVALID_LOCK);
    CHECK(aros_tty_read_break(MKBADDR(slave),NULL,1)==-1 && IoErr()==ERROR_BAD_NUMBER);
    CHECK(aros_tty_write_break(MKBADDR(slave),out,-1)==-1 && IoErr()==ERROR_BAD_NUMBER);
    memset(fill,'q',sizeof fill);
    for(cycle=0;cycle<160;++cycle) {
        struct Process *child;unsigned admitted=0;
        TtyEnd end,peer;
        operation=cycle%4;phase=(cycle/4)%4;end=operation/2?TTY_SLAVE:TTY_MASTER;
        peer=end==TTY_MASTER?TTY_SLAVE:TTY_MASTER;
        io_handle=MKBADDR(end==TTY_MASTER?master:slave);
        result=-99;error=0;remaining_signal=0;memcpy(io_buffer,"ABCDEFGH",8);
        CHECK(tty_pair_flush(&endpoint.service.pair,TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT)==TTY_OK);
        if(operation%2) CHECK(tty_pair_write(&endpoint.service.pair,end,fill,sizeof fill).count==sizeof fill);
        child=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"TTY IO interrupt client",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        child_id=GetETaskID(&child->pr_Task);CHECK(child_id);
        for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            aros_tty_packets_poll(&endpoint);
            if(!admitted) for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
                struct DosPacket *p=endpoint.slots[i].packet;
                if(p && (p->dp_Type==ACTION_READ || p->dp_Type==ACTION_WRITE)) {
                    ++admitted;Signal(&child->pr_Task,SIGBREAKF_CTRL_C);
                    if(phase==1) {
                        if(operation%2) CHECK(tty_pair_read(&endpoint.service.pair,peer,out,3).count==3 && !memcmp(out,"qqq",3));
                        else CHECK(tty_pair_write(&endpoint.service.pair,peer,"xyz",3).count==3);
                    }
                    break;
                }
            }
            Delay(1);
        }
        CHECK(admitted==(phase>=2?0u:1u));CHECK(ChildStatus(child_id)==CHILD_EXITED);
        ChildFree(child_id);child_id=0;
        if(phase==1) {
            CHECK(result==3 && !error && remaining_signal==SIGBREAKF_CTRL_C);
            if(operation%2) {
                CHECK(tty_pair_read(&endpoint.service.pair,peer,out,sizeof out).count==sizeof out);
                CHECK(!memcmp(out,fill,sizeof out-3) && !memcmp(out+sizeof out-3,"ABC",3));
            } else CHECK(!memcmp(io_buffer,"xyzDEFGH",8));
        } else {
            if(phase==3) CHECK(result==0 && !error && remaining_signal==SIGBREAKF_CTRL_C);
            else CHECK(result==-1 && error==ERROR_BREAK && !remaining_signal);
            CHECK(!memcmp(io_buffer,"ABCDEFGH",8));
            if(operation%2) CHECK(tty_pair_read(&endpoint.service.pair,peer,out,sizeof out).count==sizeof out && !memcmp(out,fill,sizeof out));
        }
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) CHECK(!endpoint.slots[i].packet);
    }
    puts("TTY IO INTERRUPT PASS: cases=160 lanes=4 cancel=40 partial_completion=40 preexisting=40 empty=40 exact bytes/signals/cleanup");fflush(stdout);
    peer_close_cases();
    cleanup();return 0;
}
