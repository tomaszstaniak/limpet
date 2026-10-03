#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyWorker *worker;
static BPTR master,slave,child_handles[3],segment;
static ULONG child_id;
static LONG child_result;
static void reap(void)
{
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) Delay(1);
    if(child_id) { ChildFree(child_id);child_id=0; }
}
static void fail(const char *expression,int line)
{
    unsigned i;
    printf("TTY CHILD FAIL line=%d: %s\n",line,expression);fflush(stdout);
    if(worker) aros_tty_worker_stop(worker);
    reap(); /* Never unload executable or shared wrapper code under a child. */
    for(i=0;i<3;++i) if(child_handles[i]) Close(child_handles[i]);
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
    unsigned char b[64];size_t offset=0;
    CHECK(length<=sizeof b);
    while(offset<length) {
        LONG n;
        CHECK(WaitForChar(master,5000000)==DOSTRUE);
        n=Read(master,b+offset,(LONG)(length-offset));CHECK(n>0);
        offset+=(size_t)n;
    }
    CHECK(!memcmp(b,expected,length));
}
#define EXPECT(s) expect(s,sizeof(s)-1)
int main(int argc,char **argv)
{
    unsigned cycle,i,tick;TtyConfig config;
    CHECK(argc==2);
    segment=LoadSeg((CONST_STRPTR)argv[1]);CHECK(segment);
    worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
    for(cycle=0;cycle<2;++cycle) {
        struct Process *child;
#ifdef XTY_CONTROL_PROBE
        TtyGeometry geometry={1,31,109,872,527};
        CHECK(aros_tty_worker_set_geometry(worker,&geometry)==TTY_OK);
#endif
        for(i=0;i<3;++i)
            CHECK(aros_tty_worker_duplicate(worker,slave,&child_handles[i])==TTY_OK);
        child_result=-1;
        child=CreateNewProcTags(NP_Entry,(IPTR)child_entry,NP_Name,(IPTR)"TTY DOS peer",
            NP_StackSize,131072,NP_Cli,TRUE,NP_NotifyOnDeath,TRUE,
            NP_Input,child_handles[0],NP_Output,child_handles[1],NP_Error,child_handles[2],
            NP_CloseInput,TRUE,NP_CloseOutput,TRUE,NP_CloseError,TRUE,TAG_DONE);
        CHECK(child);
        child_id=GetETaskID(&child->pr_Task);
        for(i=0;i<3;++i) child_handles[i]=BNULL; /* Ownership transferred. */
        Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
        EXPECT("RAW_READY\n");CHECK(Write(master,"A\0B",3)==3);
        EXPECT("COOKED_READY\n");CHECK(Write(master,"one\n",4)==4);
        EXPECT("one\nEOF_READY\n");CHECK(Write(master,"\004",1)==1);
        EXPECT("RESTART_READY\n");CHECK(Write(master,"two\n",4)==4);
        EXPECT("two\nPEER_PASS\n");
        for(tick=0;tick<500 && ChildStatus(child_id)!=CHILD_EXITED;++tick) Delay(1);
        CHECK(ChildStatus(child_id)==CHILD_EXITED);reap();CHECK(child_result==0);
        CHECK(aros_tty_worker_get_config(worker,1,&config)==TTY_OK);
        CHECK(config.local_flags==0);
#ifdef XTY_CONTROL_PROBE
        CHECK(aros_tty_worker_get_geometry(worker,1,&geometry)==TTY_OK);
        CHECK(geometry.rows==32 && geometry.columns==110 && geometry.xpixel==880 && geometry.ypixel==544);
        puts("TTY CHILD PROTOCOL PASS: shared geometry/config over duplicated standard streams");
#endif
        CHECK(WaitForChar(master,0)==DOSFALSE && IoErr()==0);
        printf("TTY CHILD CYCLE PASS: cycle=%u return=0 raw/cooked temporary EOF restart\n",cycle+1);
    }
    { BPTR h=slave;slave=BNULL;CHECK(Close(h)); }
    CHECK(WaitForChar(master,0)==DOSFALSE && IoErr()==0);
    { unsigned char b;CHECK(Read(master,&b,1)==0); }
    { BPTR h=master;master=BNULL;CHECK(Close(h)); }
    CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    UnLoadSeg(segment);segment=BNULL;
    puts("TTY CHILD PASS: standalone DOS executable, standard streams, two launches on one PTY, child stream closure, final EOF, worker reap");
    return 0;
}
