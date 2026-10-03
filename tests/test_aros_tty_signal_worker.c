#include "../src/aros_tty_worker.h"
#include "../src/aros_tty_termios.h"
#include "../src/aros_tty_client.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static ArosTtyWorker *worker;
static BPTR master,slave;
static ULONG child_id;
static struct Task *gated;
static LONG result,error;
static unsigned char data[8];
static void reader(void)
{
    Wait(SIGBREAKF_CTRL_F);
    result=aros_tty_read_break(slave,data,sizeof data);error=IoErr();
}
static void cleanup(void)
{
    if(gated) { Signal(gated,SIGBREAKF_CTRL_F);gated=NULL; }
    if(worker) aros_tty_worker_stop(worker);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) Delay(1);
    if(child_id) { ChildFree(child_id);child_id=0; }
    if(master) Close(master);
    if(slave) Close(slave);
    if(worker) aros_tty_worker_finish(worker);
}
#define CHECK(x) do { if(!(x)) { printf("TTY VINTR WORKER FAIL line=%d: %s\n",__LINE__,#x);cleanup();return 20; } } while(0)
static int capabilities(int ready)
{
    XtyControl c;memset(&c,0,sizeof c);c.version=1;c.operation=XTY_CAPS;
    return aros_tty_control(slave,&c)==DOSTRUE &&
        !!(c.values[0]&XTY_CAP_VINTR)==ready &&
        c.values[3]==(ready?255u:63u);
}
int main(void)
{
    unsigned cycle,tick;struct termios base,t,out;
    const int actions[]={TCSANOW,TCSADRAIN,TCSAFLUSH};
    worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
    CHECK(!aros_tty_tcgetattr(slave,&base));
    t=base;t.c_lflag=ISIG;t.c_cc[VINTR]=3;
    CHECK(capabilities(0));
    CHECK(aros_tty_tcsetattr(slave,TCSANOW,&t)==-1 && errno==ENOTSUP);
    for(cycle=0;cycle<12;++cycle) {
        struct Process *child;
        memset(data,'X',sizeof data);result=-99;error=0;
        child=CreateNewProcTags(NP_Entry,(IPTR)reader,NP_Name,(IPTR)"VINTR worker reader",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(child);
        gated=&child->pr_Task;child_id=GetETaskID(gated);CHECK(child_id);
        CHECK(aros_tty_worker_signal_target(worker,gated)==TTY_OK);
        CHECK(aros_tty_worker_signal_target(worker,gated)==TTY_BUSY);
        CHECK(capabilities(1));
        t=base;t.c_lflag=ISIG|((cycle&1)?NOFLSH:0);t.c_cc[VINTR]=3;
        CHECK(!aros_tty_tcsetattr(slave,actions[cycle%3],&t));
        CHECK(!aros_tty_tcgetattr(slave,&out));
        CHECK(out.c_lflag==t.c_lflag && out.c_cc[VINTR]==3);
        out.c_cc[VQUIT]=28;
        CHECK(aros_tty_tcsetattr(slave,TCSANOW,&out)==-1 && errno==ENOTSUP);
        Signal(gated,SIGBREAKF_CTRL_F);gated=NULL;
        CHECK(Write(master,"\003tail",5)==1);
        for(tick=0;tick<100 && ChildStatus(child_id)!=CHILD_EXITED;++tick) Delay(1);
        CHECK(ChildStatus(child_id)==CHILD_EXITED);
        CHECK(result==-1 && error==ERROR_BREAK && !memcmp(data,"XXXXXXXX",8));
        ChildFree(child_id);child_id=0;
        CHECK(aros_tty_worker_signal_target(worker,NULL)==TTY_OK);
        CHECK(capabilities(0));CHECK(!aros_tty_tcgetattr(slave,&out));
        CHECK(!(out.c_lflag&(ISIG|NOFLSH)) && out.c_cc[VINTR]==(cc_t)_POSIX_VDISABLE);
        CHECK(aros_tty_tcsetattr(slave,TCSANOW,&t)==-1 && errno==ENOTSUP);
    }
    cleanup();
    puts("TTY VINTR WORKER PASS: cycles=12 actions=3 capabilities, typed roundtrip, real interrupt, revoke/rebind, VQUIT rejection, cleanup");
    return 0;
}
