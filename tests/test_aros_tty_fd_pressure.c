#include "../src/aros_tty_packets.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyPackets endpoint;
static struct FileHandle *master,*slave,*handles[2];
static struct MsgPort *reply;
static struct DosPacket *packet;
static BPTR segment;
static ULONG child_id;
static LONG child_result;
static void report(const char *s) { Write(Output(),s,(LONG)strlen(s)); }
static void child_entry(void)
{
    Wait(SIGBREAKF_CTRL_F);
    child_result=RunCommand(segment,131072,(STRPTR)"\n",1);
}
static void cleanup(void)
{
    unsigned i;
    aros_tty_packets_stop(&endpoint);
    /* Keep servicing close packets until the child no longer owns our code.
       A stuck child is handled by the external private-VM watchdog. */
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
static void fail(const char *e,int line)
{
    char s[256];snprintf(s,sizeof s,"LIBC PRESSURE FAIL line=%d child=%ld: %s\n",line,(long)child_result,e);
    report(s);cleanup();exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void drain(size_t *received)
{
    unsigned char bytes[TTY_QUEUE_CAPACITY];size_t i;
    TtyResult r=tty_pair_read(&endpoint.service.pair,TTY_MASTER,bytes,sizeof bytes);
    CHECK(r.count>0);
    for(i=0;i<r.count;++i,++*received) {
        size_t pos=*received;
        unsigned char expected=pos<TTY_QUEUE_CAPACITY-7?'q':
            (unsigned char)((pos-(TTY_QUEUE_CAPACITY-7))*37u+11u);
        CHECK(pos<TTY_QUEUE_CAPACITY-7+10003 && bytes[i]==expected);
    }
}
int main(int argc,char **argv)
{
    unsigned cycle,i,tick;unsigned char fill[TTY_QUEUE_CAPACITY-7];
    if(argc!=2) return 20;
    segment=LoadSeg((CONST_STRPTR)argv[1]);if(!segment) return 20;
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);
    if(!master || !slave || !reply || !packet) return 20;
    if(aros_tty_packets_init(&endpoint,master,slave)) return 20;
    memset(fill,'q',sizeof fill);
    for(cycle=0;cycle<20;++cycle) {
        struct Process *child;size_t received=0;unsigned releases=0;char s[160];
        TtyConfig config=endpoint.service.pair.config;
        config.local_flags=0;config.input_flags=0;config.output_flags=0;
        CHECK(tty_pair_configure(&endpoint.service.pair,&config)==TTY_OK);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,fill,sizeof fill).count==sizeof fill);
        for(i=0;i<2;++i) {
            handles[i]=AllocDosObject(DOS_FILEHANDLE,NULL);CHECK(handles[i]);
            CHECK(aros_tty_packets_duplicate(&endpoint,slave->fh_Arg1,handles[i])==TTY_OK);
        }
        child_result=-99;
        child=CreateNewProcTags(NP_Entry,(IPTR)child_entry,NP_Name,(IPTR)"libc pressure peer",
            NP_StackSize,131072,NP_Cli,TRUE,NP_NotifyOnDeath,TRUE,
            NP_Input,MKBADDR(handles[0]),NP_CloseInput,TRUE,
            NP_Output,MKBADDR(handles[1]),NP_CloseOutput,TRUE,
            NP_Error,ErrorOutput(),NP_CloseError,FALSE,TAG_DONE);CHECK(child);
        child_id=GetETaskID(&child->pr_Task);
        if(!child_id) { report("LIBC PRESSURE FAIL: missing child id\n");for(;;) Delay(50); }
        handles[0]=handles[1]=NULL;Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
        for(tick=0;tick<1500 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            int waiting=0;
            aros_tty_packets_poll(&endpoint);
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(endpoint.slots[i].packet && endpoint.slots[i].packet->dp_Type==ACTION_READ) waiting=1;
            if(waiting) {
                CHECK(endpoint.service.pair.output.count==TTY_QUEUE_CAPACITY);
                drain(&received);++releases;
                CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,"G",1).count==1);
            }
            Delay(1);
        }
        CHECK(ChildStatus(child_id)==CHILD_EXITED && child_result==0);
        ChildFree(child_id);child_id=0;
        if(endpoint.service.pair.output.count) drain(&received);
        CHECK(received==sizeof fill+10003 && releases>1);
        CHECK(!endpoint.service.pair.input.count && !endpoint.service.pair.output.count);
        CHECK(endpoint.service.pair.refs[TTY_MASTER]==1 && endpoint.service.pair.refs[TTY_SLAVE]==1);
        for(i=0;i<TTY_SERVICE_HANDLES;++i) {
            TtyHandle id=endpoint.service.handles[i].id;
            CHECK(!id || id==(TtyHandle)master->fh_Arg1 || id==(TtyHandle)slave->fh_Arg1);
            CHECK(!endpoint.exports[i] || endpoint.exports[i]==(TtyHandle)master->fh_Arg1 ||
                  endpoint.exports[i]==(TtyHandle)slave->fh_Arg1);
        }
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            CHECK(!endpoint.slots[i].packet);
            CHECK(!endpoint.service.slots[i].request);
        }
        CHECK(!endpoint.timed_read && !endpoint.read_timer_token);
        snprintf(s,sizeof s,"LIBC PRESSURE CYCLE PASS: cycle=%u payload=10003 first=7 releases=%u exact=1\n",cycle+1,releases);
        report(s);
    }
    cleanup();report("LIBC PRESSURE PASS: cycles=20 payload=200060 shared flags, short write, full EAGAIN, exact retry, cleanup\n");
    return 0;
}
