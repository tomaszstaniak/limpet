#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyWorker *worker;
static BPTR master,slave,handles[3],segment;
static ULONG child_id;
static LONG child_result;

static void report(const char *text)
{ Write(Output(),text,(LONG)strlen(text)); }

static void reap(void)
{
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) Delay(1);
    if(child_id) { ChildFree(child_id);child_id=0; }
}

static void fail(const char *expression,int line)
{
    char message[256];unsigned i;
    snprintf(message,sizeof message,"TTY FD SESSION FAIL line=%d: %s\n",line,expression);
    report(message);
    if(worker) aros_tty_worker_stop(worker);
    /* Do not free code or shared state while a child may still use it.
       The host must terminate the private VM if teardown itself stalls. */
    reap();
    for(i=0;i<3;++i) if(handles[i]) Close(handles[i]);
    if(slave) Close(slave);
    if(master) Close(master);
    if(worker && aros_tty_worker_finish(worker)) for(;;) Delay(50);
    if(segment) UnLoadSeg(segment);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)

static void child_entry(void)
{
    Wait(SIGBREAKF_CTRL_F);
    child_result=RunCommand(segment,131072,(STRPTR)"\n",1);
}

static void expect(const char *expected,size_t length)
{
    unsigned char bytes[64];size_t offset=0;
    CHECK(length<=sizeof bytes);
    while(offset<length) {
        LONG count;
        CHECK(WaitForChar(master,5000000)==DOSTRUE);
        count=Read(master,bytes+offset,(LONG)(length-offset));CHECK(count>0);
        offset+=(size_t)count;
    }
    CHECK(!memcmp(bytes,expected,length));
}
#define EXPECT(s) expect(s,sizeof(s)-1)

int main(int argc,char **argv)
{
    unsigned cycle,i,tick;TtyConfig config;
    CHECK(argc==2);
    segment=LoadSeg((CONST_STRPTR)argv[1]);CHECK(segment);
    worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
    for(cycle=0;cycle<20;++cycle) {
        struct Process *child;char message[96];
        CHECK(aros_tty_worker_get_config(worker,1,&config)==TTY_OK);
        config.local_flags=TTY_CANON|TTY_ECHO;config.input_flags=TTY_ICRNL;
        config.output_flags=TTY_OPOST|TTY_ONLCR;
        CHECK(aros_tty_worker_configure(worker,&config)==TTY_OK);
#ifdef TEST_LIBC_GEOMETRY
        { TtyGeometry geometry={TTY_PAIR_VERSION,24,80,640,408};
          CHECK(aros_tty_worker_set_geometry(worker,&geometry)==TTY_OK); }
#endif
        for(i=0;i<3;++i)
            CHECK(aros_tty_worker_duplicate(worker,slave,&handles[i])==TTY_OK);
        child_result=-1;
        child=CreateNewProcTags(NP_Entry,(IPTR)child_entry,NP_Name,(IPTR)"TTY standard fd session",
            NP_StackSize,131072,NP_Cli,TRUE,NP_NotifyOnDeath,TRUE,
            NP_Input,handles[0],NP_Output,handles[1],NP_Error,handles[2],
            NP_CloseInput,TRUE,NP_CloseOutput,TRUE,NP_CloseError,TRUE,TAG_DONE);
        CHECK(child);
        child_id=GetETaskID(&child->pr_Task);
        for(i=0;i<3;++i) handles[i]=0;
        Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
        EXPECT("FD_RAW_READY\n");
#ifdef TEST_LIBC_GEOMETRY
        { TtyGeometry geometry={TTY_PAIR_VERSION,22,72,576,374};
          CHECK(aros_tty_worker_set_geometry(worker,&geometry)==TTY_OK); }
#endif
        CHECK(Write(master,"A\0B\033[A\360\237\230\200",10)==10);
        EXPECT("A\0B\033[A\360\237\230\200FD_COOKED_READY\r\n");
        CHECK(Write(master,"one\r",4)==4);
        EXPECT("one\r\nFD_EOF_READY\r\n");
        CHECK(Write(master,"\004",1)==1);
        EXPECT("FD_RESTART_READY\r\n");
        CHECK(Write(master,"two\n",4)==4);
        EXPECT("two\r\nFD_SESSION_PASS\n");
        for(tick=0;tick<500 && ChildStatus(child_id)!=CHILD_EXITED;++tick) Delay(1);
        CHECK(ChildStatus(child_id)==CHILD_EXITED);reap();CHECK(child_result==0);
        CHECK(aros_tty_worker_get_config(worker,1,&config)==TTY_OK);
        CHECK(!config.local_flags && !config.input_flags && !config.output_flags);
        CHECK(WaitForChar(master,0)==DOSFALSE && IoErr()==0);
        snprintf(message,sizeof message,"TTY FD SESSION CYCLE PASS: cycle=%u return=0\n",cycle+1);
        report(message);
    }
    { BPTR h=slave;slave=0;CHECK(Close(h)); }
    { unsigned char byte;CHECK(Read(master,&byte,1)==0); }
    { BPTR h=master;master=0;CHECK(Close(h)); }
    CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    UnLoadSeg(segment);segment=0;
    report("TTY FD SESSION PASS: cycles=20 standard read/write, binary UTF-8/ESC, raw/canonical, CR mapping, exact echo, temporary EOF/restart, child close, final EOF, worker reap\n");
#ifdef TEST_LIBC_GEOMETRY
    report("TTY LIBC GEOMETRY PASS: cycles=20 rows/cols=24/80->22/72 pixels=640/408->576/374 no ioctl wrapper\n");
#else
    report("TTY FD GEOMETRY GAP: public TIOCGWINSZ returns ENOTTY unchanged\n");
#endif
    return 0;
}
