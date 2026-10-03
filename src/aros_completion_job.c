#include "aros_completion_job.h"
#include "aros_completion.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>

static void worker(void)
{
    ArosCompletionJob *j=(ArosCompletionJob *)FindTask(NULL)->tc_UserData;
    /* Parent captures GetETaskID before releasing us, even on SMP. */
    Wait(SIGBREAKF_CTRL_F);
#ifdef XTERM_TEST_SLOW_COMPLETION
    /* TEST BUILDS ONLY: widen the window in which F5 and close must wait. */
    Delay(250);
#endif
    j->result=aros_completion_collect(j->completion,j->reader);
}
int aros_completion_job_start(ArosCompletionJob *j,XCompletion *c,struct Process *reader)
{
    struct Process *child;
    if(!j || j->task_id || !c || !reader) return -1;
    j->completion=c; j->reader=reader; j->result=0;
    child=CreateNewProcTags(NP_Entry,(IPTR)worker,NP_UserData,(IPTR)j,
        NP_Name,(IPTR)"AROS XTerm completion",NP_StackSize,65536,
        NP_NotifyOnDeath,TRUE,NP_Input,0,NP_Output,0,NP_Error,0,
        NP_WindowPtr,(IPTR)-1,
        /* Explicit dirs: by default CreateNewProc calls Lock("") and
         * DupLock(pr_HomeDir) in the parent, which are synchronous DOS
         * calls on its process port (ABIv11 rom/dos/createnewproc.c). The
         * child enumerates relative to the reader's dirs, not its own. */
        NP_CurrentDir,0,NP_HomeDir,0,TAG_DONE);
    if(!child) return -1;
    j->task_id=GetETaskID(&child->pr_Task);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    return 0;
}
int aros_completion_job_poll(ArosCompletionJob *j)
{
    if(!j || !j->task_id || ChildStatus(j->task_id)!=CHILD_EXITED) return 0;
    ChildFree(j->task_id); j->task_id=0;
    return 1;
}
