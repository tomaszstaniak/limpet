#include "../src/aros_tty_signal.h"
#include "../src/aros_tty_packets.h"
#include "../src/aros_tty_client.h"
#include "../src/aros_tty_termios.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static ArosTtyPackets endpoint;
static ArosTtySignal route;
static struct FileHandle *master,*slave;
static struct MsgPort *reply;
static struct DosPacket *write_packet;
static ULONG child_id;
static struct Task *gated_child;
static LONG read_result,read_error;
static unsigned setter;
static unsigned char buffer[8];
static void cleanup(void)
{
    unsigned i;
    aros_tty_signal_close(&route);
    if(gated_child) { Signal(gated_child,SIGBREAKF_CTRL_F);gated_child=NULL; }
    aros_tty_packets_stop(&endpoint);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) {
        aros_tty_packets_poll(&endpoint);Delay(1);
    }
    if(child_id) { ChildFree(child_id);child_id=0; }
    while(GetMsg(reply)) {}
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(endpoint.exports[i]) {
        write_packet->dp_Type=ACTION_END;write_packet->dp_Arg1=endpoint.exports[i];
        SendPkt(write_packet,endpoint.ports[endpoint.export_ends[i]],reply);
        aros_tty_packets_poll(&endpoint);WaitPort(reply);GetMsg(reply);
    }
    if(aros_tty_packets_dispose(&endpoint)) exit(20);
    FreeDosObject(DOS_STDPKT,write_packet);DeleteMsgPort(reply);
    FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);
}
static void fail(const char *expression,int line)
{
    printf("TTY INPUT SIGNAL FAIL line=%d: %s\n",line,expression);fflush(stdout);
    cleanup();exit(20);
}
#define CHECK(x) do { if(!(x)) fail(#x,__LINE__); } while(0)
static void reader(void)
{
    Wait(SIGBREAKF_CTRL_F);
    if(setter) {
        struct termios t;
        if(aros_tty_tcgetattr(MKBADDR(slave),&t)) { read_result=-2;return; }
        t.c_lflag=ISIG;t.c_cc[VINTR]=3;
        read_result=aros_tty_tcsetattr(MKBADDR(slave),setter==1?TCSADRAIN:TCSAFLUSH,&t);
        read_error=errno;return;
    }
    read_result=aros_tty_read_break(MKBADDR(slave),buffer,sizeof buffer);
    read_error=IoErr();
}
int main(void)
{
    unsigned cycle,tick,i;unsigned char full[TTY_QUEUE_CAPACITY];
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    reply=CreateMsgPort();write_packet=AllocDosObject(DOS_STDPKT,NULL);
    if(!master || !slave || !reply || !write_packet) return 20;
    if(aros_tty_packets_init(&endpoint,master,slave)) return 20;
    memset(full,'q',sizeof full);
    for(cycle=0;cycle<20;++cycle) {
        struct Process *child;TtyConfig c=endpoint.service.pair.config;int admitted=0,replied=0;
        read_result=-99;read_error=0;memset(buffer,'X',sizeof buffer);
        child=CreateNewProcTags(NP_Entry,(IPTR)reader,NP_Name,(IPTR)"TTY VINTR child",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        gated_child=&child->pr_Task;child_id=GetETaskID(gated_child);CHECK(child_id);
        CHECK(aros_tty_signal_open(&route,&endpoint.service,gated_child)==TTY_OK);
        CHECK(aros_tty_signal_open(&route,&endpoint.service,gated_child)==TTY_BUSY);
        c.local_flags=TTY_ISIG|((cycle&1)?TTY_NOFLSH:0);c.intr=4;
        CHECK(tty_pair_configure(&endpoint.service.pair,&c)==TTY_OK);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,full,sizeof full).count==sizeof full);
        CHECK(tty_pair_output_flow(&endpoint.service.pair,1)==TTY_OK);
        Signal(gated_child,SIGBREAKF_CTRL_F);gated_child=NULL;
        for(tick=0;tick<100 && !admitted;++tick) {
            aros_tty_packets_poll(&endpoint);
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(endpoint.slots[i].packet && endpoint.slots[i].packet->dp_Type==ACTION_READ)
                    admitted=1;
            if(!admitted) Delay(1);
        }
        CHECK(admitted);
        write_packet->dp_Type=ACTION_WRITE;write_packet->dp_Arg1=master->fh_Arg1;
        write_packet->dp_Arg2=(SIPTR)"\003tail";write_packet->dp_Arg3=5;
        SendPkt(write_packet,master->fh_Type,reply);
        for(tick=0;tick<100 && (!replied || ChildStatus(child_id)!=CHILD_EXITED);++tick) {
            aros_tty_packets_poll(&endpoint);
            if(GetMsg(reply)) replied=1;
            Delay(1);
        }
        CHECK(replied && ChildStatus(child_id)==CHILD_EXITED);
        CHECK(write_packet->dp_Res1==1 && !write_packet->dp_Res2);
        CHECK(read_result==-1 && read_error==ERROR_BREAK);
        CHECK(!memcmp(buffer,"XXXXXXXX",8));
        CHECK(route.delivered==1 && !route.dropped && !aros_break_ready(&route.gate));
        CHECK(endpoint.service.pair.output.count==((cycle&1)?sizeof full:0));
        ChildFree(child_id);child_id=0;
        /* A removed target must never receive another signal through a stale pointer. */
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,"\003",1).count==1);
        tty_service_pump(&endpoint.service);CHECK(route.delivered==1 && route.dropped==1);
        /* Revocation discards a not-yet-dispatched event before next binding. */
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,"\003",1).count==1);
        aros_tty_signal_close(&route);
        CHECK(!endpoint.service.interrupt && !endpoint.service.pair.interrupt_pending);
        CHECK(!(endpoint.service.pair.config.local_flags&TTY_ISIG));
        CHECK(tty_pair_flush(&endpoint.service.pair,TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT)==TTY_OK);
        CHECK(tty_pair_output_flow(&endpoint.service.pair,0)==TTY_OK);
    }
    for(setter=1;setter<=2;++setter) {
        struct Process *child;int admitted=0;TtyConfig saved;
        read_result=-99;read_error=0;
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,"out",3).count==3);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,"keep",4).count==4);
        child=CreateNewProcTags(NP_Entry,(IPTR)reader,NP_Name,(IPTR)"VINTR stale setter",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        gated_child=&child->pr_Task;child_id=GetETaskID(gated_child);CHECK(child_id);
        CHECK(aros_tty_signal_open(&route,&endpoint.service,gated_child)==TTY_OK);
        Signal(gated_child,SIGBREAKF_CTRL_F);gated_child=NULL;
        for(tick=0;tick<100 && !admitted;++tick) {
            aros_tty_packets_poll(&endpoint);
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(endpoint.slots[i].packet && endpoint.slots[i].packet->dp_Type==XTY_ACTION_CONTROL)
                    admitted=1;
            if(!admitted) Delay(1);
        }
        CHECK(admitted);
        aros_tty_signal_close(&route);
        CHECK(aros_tty_signal_open(&route,&endpoint.service,FindTask(NULL))==TTY_OK);
        saved=endpoint.service.pair.config;
        CHECK(tty_pair_flush(&endpoint.service.pair,TTY_FLUSH_OUTPUT)==TTY_OK);
        for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            aros_tty_packets_poll(&endpoint);Delay(1);
        }
        CHECK(ChildStatus(child_id)==CHILD_EXITED && read_result==-1 && read_error==ENOTTY);
        CHECK(!memcmp(&saved,&endpoint.service.pair.config,sizeof saved));
        CHECK(tty_pair_read(&endpoint.service.pair,TTY_SLAVE,buffer,4).count==4);
        CHECK(!memcmp(buffer,"keep",4));
        ChildFree(child_id);child_id=0;aros_tty_signal_close(&route);
    }
    puts("TTY VINTR STALE PASS: actions=2 admitted setter, unbind/rebind, rejected before configuration or input discard");
    cleanup();
    puts("TTY INPUT SIGNAL PASS: cycles=20 packet VINTR, exact short write, interrupted read, unchanged buffer, flush/noflush, removed target, revoke/rebind, cleanup");
    return 0;
}
