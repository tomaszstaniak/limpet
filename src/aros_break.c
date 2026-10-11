#include "aros_break.h"
#ifdef XTERM_NO_TASK_NOTIFY
#include <exec/execbase.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <string.h>

/* ABIv11 has no task-removal notification. The system console (con_handler)
 * keeps the task that sent its last packet and Signals it on Ctrl-C without
 * any check. Keep the same target, but Signal it only while the pointer is
 * still on exec's ready or waiting list, compared under Disable() without
 * dereferencing it: a task that has ended is never written to. A task that
 * is running on another CPU (SMP builds) is not on those lists; then the
 * break is not sent and the key can be pressed again. A new task at a reused
 * address would get the break, as with the system console. */
static int task_listed(struct Task *task)
{
    struct Node *n;
    for(n=SysBase->TaskReady.lh_Head;n->ln_Succ;n=n->ln_Succ) if(n==&task->tc_Node) return 1;
    for(n=SysBase->TaskWait.lh_Head;n->ln_Succ;n=n->ln_Succ) if(n==&task->tc_Node) return 1;
    return 0;
}
void aros_break_open(ArosBreak *b) { memset(b,0,sizeof *b); InitSemaphore(&b->gate); b->registered=1; }
void aros_break_bind(ArosBreak *b,struct Task *task)
{
    if(!b->registered) return;
    ObtainSemaphore(&b->gate); b->target=task; ReleaseSemaphore(&b->gate);
}
int aros_break_ready(ArosBreak *b)
{
    int ready;
    if(!b->registered) return 0;
    ObtainSemaphore(&b->gate); ready=b->target!=NULL; ReleaseSemaphore(&b->gate);
    return ready;
}
int aros_break_send(ArosBreak *b)
{
    int sent=0;
    if(!b->registered) return 0;
    ObtainSemaphore(&b->gate);
    if(b->target) {
        Disable();
        if(task_listed(b->target)) { Signal(b->target,SIGBREAKF_CTRL_C); sent=1; }
        Enable();
    }
    ReleaseSemaphore(&b->gate);
    return sent;
}

/* Called with the task protected (listed under Disable, or held by the
   removal gate under Forbid). Copies a bounded name; keeps no pointer. */
static void read_foreground(struct Task *task,XForeground *out)
{
    struct CommandLineInterface *cli;
    const char *name;
    size_t i;
    if(task->tc_Node.ln_Type!=NT_PROCESS) return;
    cli=BADDR(((struct Process *)task)->pr_CLI);
    if(!cli) return;
    out->known=1;
    out->running=cli->cli_Module!=BNULL;
    out->module=(unsigned long)(IPTR)cli->cli_Module;
    name=task->tc_Node.ln_Name;
    if(out->running && name)
        for(i=0;i+1<sizeof out->name && name[i];++i) out->name[i]=name[i];
}
int aros_break_foreground(ArosBreak *b,XForeground *out)
{
    memset(out,0,sizeof *out);
    if(!b->registered) return 0;
    ObtainSemaphore(&b->gate);
    if(b->target) {
        Disable();
        if(task_listed(b->target)) read_foreground(b->target,out);
        Enable();
    }
    ReleaseSemaphore(&b->gate);
    return out->known;
}
void aros_break_close(ArosBreak *b) { b->registered=0; b->target=NULL; }
#else
#include <aros/asmcall.h>
#include <exec/resident.h>
#include <resources/task.h>
#include <proto/exec.h>
#include <proto/task.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <string.h>
#include <stdio.h>

AROS_UFH3(IPTR, break_notify,
    AROS_UFHA(struct Hook *,hook,A0),
    AROS_UFHA(APTR,object,A2),
    AROS_UFHA(struct TaskNotifyMsg *,msg,A1))
{
    AROS_USERFUNC_INIT
    ArosBreak *b=hook->h_Data;
    (void)object;
    if(msg->tnm_Action==TNA_REMOVED || msg->tnm_Action==TNA_FAILED) {
        ObtainSemaphore(&b->gate);
        if(b->target==msg->tnm_Task) b->target=NULL;
        ReleaseSemaphore(&b->gate);
    }
    return 0;
    AROS_USERFUNC_EXIT
}

void aros_break_open(ArosBreak *b)
{
    struct Resident *resident;
    unsigned major,minor;
    APTR TaskResBase;
    memset(b,0,sizeof *b); InitSemaphore(&b->gate);
    TaskResBase=OpenResource((CONST_STRPTR)"task.resource");
    /* Resource startup initializes only Node, not Library version fields.
     * Mainline's resident ID carries the revision that introduced these LVOs. */
    resident=FindResident((CONST_STRPTR)"task.resource");
    if(!TaskResBase || !resident || resident->rt_Type!=NT_RESOURCE ||
       !resident->rt_IdString ||
       sscanf((const char *)resident->rt_IdString,"task.resource %u.%u",&major,&minor)!=2 ||
       resident->rt_Version!=major || major<1 || (major==1 && minor<4)) return;
    b->resource=TaskResBase;
    b->hook.h_Entry=(HOOKFUNC)break_notify; b->hook.h_Data=b;
    b->registered=AddTaskNotifyHook(&b->hook)!=0;
}
void aros_break_bind(ArosBreak *b,struct Task *task)
{
    if(!b->registered) return;
    ObtainSemaphore(&b->gate); b->target=task; ReleaseSemaphore(&b->gate);
}
int aros_break_ready(ArosBreak *b)
{
    int ready;
    if(!b->registered) return 0;
    ObtainSemaphore(&b->gate); ready=b->target!=NULL; ReleaseSemaphore(&b->gate);
    return ready;
}
int aros_break_send(ArosBreak *b)
{
    int sent=0;
    if(!b->registered) return 0;
    /* Removal notification acquires this same gate before task memory is freed. */
    ObtainSemaphore(&b->gate);
    if(b->target) { Signal(b->target,SIGBREAKF_CTRL_C); sent=1; }
    ReleaseSemaphore(&b->gate);
    return sent;
}

/* Called with the task protected (listed under Disable, or held by the
   removal gate under Forbid). Copies a bounded name; keeps no pointer. */
static void read_foreground(struct Task *task,XForeground *out)
{
    struct CommandLineInterface *cli;
    const char *name;
    size_t i;
    if(task->tc_Node.ln_Type!=NT_PROCESS) return;
    cli=BADDR(((struct Process *)task)->pr_CLI);
    if(!cli) return;
    out->known=1;
    out->running=cli->cli_Module!=BNULL;
    out->module=(unsigned long)(IPTR)cli->cli_Module;
    name=task->tc_Node.ln_Name;
    if(out->running && name)
        for(i=0;i+1<sizeof out->name && name[i];++i) out->name[i]=name[i];
}
int aros_break_foreground(ArosBreak *b,XForeground *out)
{
    memset(out,0,sizeof *out);
    if(!b->registered) return 0;
    /* Removal notification takes this gate before the task is freed. */
    ObtainSemaphore(&b->gate);
    if(b->target) { Forbid(); read_foreground(b->target,out); Permit(); }
    ReleaseSemaphore(&b->gate);
    return out->known;
}
void aros_break_close(ArosBreak *b)
{
    APTR TaskResBase=b->resource;
    /* Do not hold gate: unregister waits for any in-flight callback. */
    if(b->registered) RemTaskNotifyHook(&b->hook);
    b->registered=0; b->target=NULL; b->resource=NULL;
}
#endif
