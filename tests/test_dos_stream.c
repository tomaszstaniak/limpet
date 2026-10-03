/* Mainline transport experiment: real C:Type, two DOS pipes, exact bytes and
 * EOF. Deliberately bounded below pipe capacity; NOT a general async adapter. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>

static LONG command_result=-1;
static void command(void)
{
    Wait(SIGBREAKF_CTRL_F);
    command_result=SystemTags((CONST_STRPTR)"C:Type IN:",SYS_Input,Input(),SYS_Output,Output(),
                             SYS_Asynch,FALSE,TAG_DONE);
}
int main(void)
{
    static const unsigned char payload[]="\033[32mDOS process stream\033[0m\r\nZażółć 中文 😀 👨‍👩‍👧‍👦\r\n";
    char in_name[48],out_name[48];
    BPTR iw=0,ir=0,ow=0,or=0;
    struct Process *child,*self=(struct Process *)FindTask(NULL);
    APTR old_window=self->pr_WindowPtr;
    unsigned char got[sizeof payload+1]; size_t total=0;
    ULONG id=0; LONG n; int failed=1,started=0;
    self->pr_WindowPtr=(APTR)-1; /* Fail absent PIPE: without a volume requester. */
    snprintf(in_name,sizeof in_name,"PIPE:xt-%08lx-in/4096",(unsigned long)GetETaskID(self));
    snprintf(out_name,sizeof out_name,"PIPE:xt-%08lx-out/4096",(unsigned long)GetETaskID(self));
    iw=Open((CONST_STRPTR)in_name,MODE_NEWFILE); if(!iw) goto done;
    ir=Open((CONST_STRPTR)in_name,MODE_OLDFILE); if(!ir) goto done;
    ow=Open((CONST_STRPTR)out_name,MODE_NEWFILE); if(!ow) goto done;
    or=Open((CONST_STRPTR)out_name,MODE_OLDFILE); if(!or) goto done;
    child=CreateNewProcTags(NP_Entry,(IPTR)command,NP_Name,(IPTR)"XTerm DOS stream probe",
        NP_Input,ir,NP_Output,ow,NP_Error,0,NP_CloseInput,TRUE,NP_CloseOutput,TRUE,
        NP_NotifyOnDeath,TRUE,NP_WindowPtr,(IPTR)-1,NP_StackSize,65536,TAG_DONE);
    if(!child) goto done;
    started=1;
    ir=ow=0; /* Ownership transferred only after successful creation. */
    id=GetETaskID(child); Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    n=Write(iw,payload,sizeof payload-1);
    Close(iw); iw=0; /* This is input EOF, not Ctrl-D bytes. */
    if(n!=(LONG)sizeof payload-1) goto reap;
    while(total<sizeof got && (n=Read(or,got+total,sizeof got-total))>0) total+=(size_t)n;
    if(n==0 && total==sizeof payload-1 && !memcmp(got,payload,total)) failed=0;
reap:
    if(or) { Close(or); or=0; }
    ChildWait(id); ChildFree(id); id=0;
    if(command_result!=0) failed=1;
done:
    if(iw) Close(iw);
    if(ir) Close(ir);
    if(ow) Close(ow);
    if(or) Close(or);
    self->pr_WindowPtr=old_window;
    printf("DOS stream probe: %s, command=%ld, bytes=%lu/%lu, child reaped=%s\n",
        failed?"FAIL":"PASS",(long)command_result,(unsigned long)total,
        (unsigned long)sizeof payload-1,!started?"not started":id?"no":"yes");
    return failed?20:0;
}
