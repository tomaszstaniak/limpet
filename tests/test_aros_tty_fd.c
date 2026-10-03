#include "../src/aros_tty_worker.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <string.h>
static BPTR segment;
static LONG result;
static void run(void)
{
    Wait(SIGBREAKF_CTRL_F);result=RunCommand(segment,131072,(STRPTR)"\n",1);
}
int main(int argc,char **argv)
{
    BPTR master=0,slave=0,input=0;ArosTtyWorker *w;TtyConfig c;
    struct Process *child;ULONG id;unsigned tick;int failed=0;
    if(argc!=2 || !(segment=LoadSeg((CONST_STRPTR)argv[1]))) return 20;
    w=aros_tty_worker_start(&master,&slave);if(!w) { UnLoadSeg(segment);return 20; }
    if(aros_tty_worker_get_config(w,1,&c)!=TTY_OK) { failed=1;goto done; }
    c.local_flags=TTY_CANON|TTY_ECHO;c.input_flags=TTY_ICRNL;c.output_flags=TTY_OPOST|TTY_ONLCR;
    if(aros_tty_worker_configure(w,&c)!=TTY_OK ||
       aros_tty_worker_duplicate(w,slave,&input)!=TTY_OK) { failed=1;goto done; }
    child=CreateNewProcTags(NP_Entry,(IPTR)run,NP_Name,(IPTR)"TTY fd consumer",
        NP_StackSize,131072,NP_Cli,TRUE,NP_NotifyOnDeath,TRUE,
        NP_Input,input,NP_CloseInput,TRUE,NP_Output,Output(),NP_CloseOutput,FALSE,
        NP_Error,ErrorOutput(),NP_CloseError,FALSE,TAG_DONE);
    if(!child) { failed=1;goto done; }
    input=0;id=GetETaskID(&child->pr_Task);Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    for(tick=0;tick<500 && ChildStatus(id)!=CHILD_EXITED;++tick) Delay(1);
    if(ChildStatus(id)!=CHILD_EXITED) { failed=1;aros_tty_worker_stop(w); }
    while(ChildStatus(id)!=CHILD_EXITED) Delay(1);
    ChildFree(id);if(result) failed=1;
    {
        const char *message=result?"TTY FD CHILD RETURN: FAIL\n":"TTY FD CHILD RETURN: PASS\n";
        Write(Output(),message,(LONG)strlen(message));
    }
done:
    aros_tty_worker_stop(w);
    if(input) Close(input);
    Close(master);Close(slave);
    if(aros_tty_worker_finish(w)) failed=1;
    UnLoadSeg(segment);
    {
        const char *message=failed?"TTY FD LAUNCH FAIL: worker cleanup\n":"TTY FD LAUNCH PASS: worker cleanup\n";
        Write(Output(),message,(LONG)strlen(message));
    }
    return failed?20:0;
}
