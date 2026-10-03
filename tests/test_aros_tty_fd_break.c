#include "../src/aros_tty_signal.h"
#include "../src/aros_tty_packets.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyPackets endpoint;
static ArosTtySignal route;
static struct FileHandle *master,*slave,*handles[2];
static struct MsgPort *reply;
static struct DosPacket *packet;
static BPTR segment;
static ULONG child_id;
static struct Task *gated_child;
static LONG child_result;

static void report(const char *message)
{ Write(Output(),message,(LONG)strlen(message)); }
static void child_entry(void)
{
    Wait(SIGBREAKF_CTRL_F);
    child_result=RunCommand(segment,131072,(STRPTR)"\n",1);
}
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
        packet->dp_Type=ACTION_END;packet->dp_Arg1=endpoint.exports[i];
        SendPkt(packet,endpoint.ports[endpoint.export_ends[i]],reply);
        aros_tty_packets_poll(&endpoint);WaitPort(reply);GetMsg(reply);
    }
    if(aros_tty_packets_dispose(&endpoint)) exit(20);
    for(i=0;i<2;++i) if(handles[i]) FreeDosObject(DOS_FILEHANDLE,handles[i]);
    FreeDosObject(DOS_STDPKT,packet);DeleteMsgPort(reply);
    FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);
    UnLoadSeg(segment);
}
static void fail(const char *expression,int line)
{
    char text[256];
    snprintf(text,sizeof text,"TTY FD BREAK FAIL line=%d child=%ld: %s\n",line,(long)child_result,expression);
    report(text);cleanup();exit(20);
}
#define CHECK(x) do { if(!(x)) fail(#x,__LINE__); } while(0)

int main(int argc,char **argv)
{
    unsigned cycle,tick,i;int writing;unsigned char full[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];
    if(argc!=3) return 20;
    writing=!strcmp(argv[2],"write");
    segment=LoadSeg((CONST_STRPTR)argv[1]);if(!segment) return 20;
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);
    if(!master || !slave || !reply || !packet) return 20;
    if(aros_tty_packets_init(&endpoint,master,slave)) return 20;
    memset(full,'q',sizeof full);
    for(cycle=0;cycle<10;++cycle) {
        struct Process *child;TtyConfig config=endpoint.service.pair.config;
        int admitted=0,replied=0;char text[128];
        for(i=0;i<2;++i) {
            handles[i]=AllocDosObject(DOS_FILEHANDLE,NULL);CHECK(handles[i]);
            CHECK(aros_tty_packets_duplicate(&endpoint,slave->fh_Arg1,handles[i])==TTY_OK);
        }
        child_result=-99;
        child=CreateNewProcTags(NP_Entry,(IPTR)child_entry,NP_Name,(IPTR)"TTY fd interrupt peer",
            NP_StackSize,131072,NP_Cli,TRUE,NP_NotifyOnDeath,TRUE,
            NP_Input,MKBADDR(handles[0]),NP_CloseInput,TRUE,
            NP_Output,MKBADDR(handles[1]),NP_CloseOutput,TRUE,
            NP_Error,ErrorOutput(),NP_CloseError,FALSE,TAG_DONE);CHECK(child);
        child_id=GetETaskID(&child->pr_Task);
        /* Missing lifetime tracking cannot safely unload the gated child. */
        if(!child_id) { report("TTY FD BREAK FAIL: missing child id\n");for(;;) Delay(50); }
        handles[0]=handles[1]=NULL;
        gated_child=&child->pr_Task;
        CHECK(aros_tty_signal_open(&route,&endpoint.service,gated_child)==TTY_OK);
        config.local_flags=TTY_ISIG|TTY_NOFLSH;config.intr=4;
        CHECK(tty_pair_configure(&endpoint.service.pair,&config)==TTY_OK);
        if(writing) CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,full,sizeof full).count==sizeof full);
        Signal(gated_child,SIGBREAKF_CTRL_F);gated_child=NULL;
        for(tick=0;tick<250 && !admitted;++tick) {
            aros_tty_packets_poll(&endpoint);
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(endpoint.slots[i].packet && endpoint.slots[i].packet->dp_Type==(writing?ACTION_WRITE:ACTION_READ)) admitted=1;
            if(!admitted) Delay(1);
        }
        CHECK(admitted);
        packet->dp_Type=ACTION_WRITE;packet->dp_Arg1=master->fh_Arg1;
        packet->dp_Arg2=(SIPTR)"\003";packet->dp_Arg3=1;
        SendPkt(packet,master->fh_Type,reply);
        for(tick=0;tick<250 && (!replied || ChildStatus(child_id)!=CHILD_EXITED);++tick) {
            aros_tty_packets_poll(&endpoint);if(GetMsg(reply)) replied=1;Delay(1);
        }
        CHECK(replied && packet->dp_Res1==1 && !packet->dp_Res2);
        CHECK(ChildStatus(child_id)==CHILD_EXITED);
        CHECK(child_result==0 && route.delivered==1 && !route.dropped);
        ChildFree(child_id);child_id=0;aros_tty_signal_close(&route);
        if(writing) {
            CHECK(tty_pair_read(&endpoint.service.pair,TTY_MASTER,out,sizeof out).count==sizeof out);
            CHECK(!memcmp(out,full,sizeof out));
        }
        CHECK(!endpoint.service.pair.output.count && !endpoint.service.pair.input.count);
        snprintf(text,sizeof text,"TTY FD %s BREAK CYCLE PASS: cycle=%u admitted-before-VINTR EINTR exact buffers\n",writing?"WRITE":"READ",cycle+1);
        report(text);
    }
    cleanup();
    report(writing?"TTY FD WRITE BREAK PASS: cycles=10 full output, noflush, access/errors, NIL fallback, cleanup\n":
                   "TTY FD READ BREAK PASS: cycles=10 empty input, unchanged buffer, access/errors, NIL fallback, cleanup\n");
#ifdef TEST_LIBC_NONBLOCK
    report("TTY LIBC FLAGS PASS: set/clear after EINTR, no syscall wrappers\n");
#else
    report("TTY FD NONBLOCK GAP: F_SETFL rejected, flags unchanged; no nonblocking I/O acceptance\n");
#endif
    return 0;
}
