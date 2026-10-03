#include "aros_clip_job.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <string.h>

static void worker(void)
{
    ArosClipJob *j=(ArosClipJob *)FindTask(NULL)->tc_UserData;
    /* Parent captures GetETaskID before releasing us, even on SMP. */
    Wait(SIGBREAKF_CTRL_F);
    if(j->writing) j->result=aros_clip_write(j->unit,j->bytes,j->length);
    else j->result=aros_clip_read(j->unit,j->bytes,sizeof j->bytes,&j->length);
}
int aros_clip_job_start(ArosClipJob *j,int w,unsigned u,const unsigned char *b,size_t n)
{
    struct Process *child;
    if(!j || j->task_id || u>255 || n>sizeof j->bytes || (n && !b)) return -1;
    j->writing=!!w; j->unit=u; j->length=n; j->result=-1;
    if(n) memcpy(j->bytes,b,n);
    child=CreateNewProcTags(NP_Entry,(IPTR)worker,NP_UserData,(IPTR)j,
        NP_Name,(IPTR)"AROS XTerm clipboard",NP_StackSize,65536,
        NP_NotifyOnDeath,TRUE,NP_Input,0,NP_Output,0,NP_Error,0,
        NP_WindowPtr,(IPTR)-1,
#ifdef XTERM_PROCESS_PORT_CLIENTS
        /* Explicit dirs: by default CreateNewProc calls Lock("") and
         * DupLock(pr_HomeDir) in the parent, which are synchronous DOS
         * calls on its process port (ABIv11 rom/dos/createnewproc.c). */
        NP_CurrentDir,0,NP_HomeDir,0,
#endif
        TAG_DONE);
    if(!child) return -1;
    j->task_id=GetETaskID(&child->pr_Task);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    return 0;
}
int aros_clip_job_poll(ArosClipJob *j)
{
    if(!j || !j->task_id || ChildStatus(j->task_id)!=CHILD_EXITED) return 0;
    ChildFree(j->task_id); j->task_id=0;
    return 1;
}
