#include "../src/aros_tty_packets.h"
#include "../src/aros_tty_termios.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ArosTtyPackets endpoint;
static struct FileHandle *master,*slave;
static ULONG child_id;
static unsigned operation,phase;
static ULONG remaining_signal;
static int result,error;
static struct termios target;
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
}
static void fail(const char *expr,int line)
{
    printf("TTY INTERRUPT FAIL line=%d: %s\n",line,expr);fflush(stdout);
    cleanup();exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void client(void)
{
    if(phase==2) Signal(FindTask(NULL),SIGBREAKF_CTRL_C);
    result=operation==0?aros_tty_tcdrain(MKBADDR(slave)):
        aros_tty_tcsetattr(MKBADDR(slave),operation==1?TCSADRAIN:TCSAFLUSH,&target);
    error=errno;
    remaining_signal=SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C;
}
int main(void)
{
    unsigned cycle,tick,i;unsigned char out[8];TtyConfig before,raw;
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(master && slave);CHECK(!aros_tty_packets_init(&endpoint,master,slave));
    raw=endpoint.service.pair.config;
    memset(&target,0,sizeof target);target.c_cflag=CS8|CREAD|CLOCAL;
    memset(target.c_cc,(unsigned char)_POSIX_VDISABLE,sizeof target.c_cc);
    cfsetispeed(&target,B38400);cfsetospeed(&target,B38400);
    target.c_cc[VMIN]=1;target.c_cc[VTIME]=0;target.c_cc[VEOF]=4;target.c_oflag=OPOST|ONLCR;
    for(cycle=0;cycle<126;++cycle) {
        struct Process *child;unsigned admitted=0,second=0;
        operation=cycle%3;phase=cycle<60?0:cycle<120?1:cycle<123?2:3;
        result=-99;error=0;remaining_signal=0;
        CHECK(tty_pair_configure(&endpoint.service.pair,&raw)==TTY_OK);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_SLAVE,"old",3).count==3);
        CHECK(tty_pair_write(&endpoint.service.pair,TTY_MASTER,"keep",4).count==4);
        before=endpoint.service.pair.config;
        child=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"TTY interrupt client",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        child_id=GetETaskID(&child->pr_Task);CHECK(child_id);
        for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            if(phase==3 && admitted && !second) {
                struct Message *m=GetMsg(endpoint.ports[TTY_SLAVE]);
                if(!m) { Delay(1);continue; }
                if(m) {
                    struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
                    int is_cancel=p->dp_Type==XTY_ACTION_CANCEL;
                    PutMsg(endpoint.ports[TTY_SLAVE],m);
                    CHECK(is_cancel);
                    Signal(&child->pr_Task,SIGBREAKF_CTRL_C);++second;
                }
            }
            aros_tty_packets_poll(&endpoint);
            if(!admitted) for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
                if(endpoint.slots[i].packet && endpoint.slots[i].packet->dp_Type==XTY_ACTION_CONTROL) {
                    ++admitted;Signal(&child->pr_Task,SIGBREAKF_CTRL_C);
                    /* Retire output before the next owner poll, so completion
                       wins even if the child's cancel packet is now queued. */
                    if(phase==1) CHECK(tty_pair_read(&endpoint.service.pair,TTY_MASTER,out,sizeof out).count==3 && !memcmp(out,"old",3));
                    break;
                }
            }
            Delay(1);
        }
        CHECK(admitted==(phase==2?0u:1u));CHECK(ChildStatus(child_id)==CHILD_EXITED);
        ChildFree(child_id);child_id=0;
        if(phase==1) {
            CHECK(result==0 && remaining_signal==SIGBREAKF_CTRL_C);
            CHECK(endpoint.service.pair.config.output_flags==(operation?TTY_OPOST|TTY_ONLCR:0));
        } else {
            CHECK(result==-1 && error==EINTR);
            CHECK(remaining_signal==(phase==3?SIGBREAKF_CTRL_C:0));
            CHECK(second==(phase==3?1u:0u));
            CHECK(!memcmp(&before,&endpoint.service.pair.config,sizeof before));
            CHECK(tty_pair_read(&endpoint.service.pair,TTY_MASTER,out,sizeof out).count==3 && !memcmp(out,"old",3));
        }
        if(phase==1 && operation==2) CHECK(!endpoint.service.pair.input.count);
        else CHECK(tty_pair_read(&endpoint.service.pair,TTY_SLAVE,out,sizeof out).count==4 && !memcmp(out,"keep",4));
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) CHECK(!endpoint.slots[i].packet);
    }
    puts("TTY INTERRUPT PASS: cancel=60 completion_race=60 preexisting=3 repeated_signal=3 actions=3 EINTR/preserved signal, exact bytes/config, cleanup");fflush(stdout);
    cleanup();return 0;
}
