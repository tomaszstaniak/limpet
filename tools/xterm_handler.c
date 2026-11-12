/* DOS caches this segment. Keep ALL mutable state on the invocation's stack;
 * no CRT, autoinit, mutable globals or shared worker segment. */
#include "../src/aros_handler.h"
#include "../src/console_name.h"
#include "../src/limpet_con.h"
#include <aros/asmcall.h>
#include <dos/dostags.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* The GUI worker beside this launcher. The ABIv11 LIMPET: handler is built
 * with -DXHANDLER_WORKER=\"limpet-console\" (Makefile abiv11-handler). */
#ifndef XHANDLER_WORKER
#define XHANDLER_WORKER "aros-xterm-worker"
#endif

/* The system console handler, to start for an open Limpet cannot serve. */
typedef struct {
    BPTR seglist;
    ULONG stack;
    LONG priority;
    struct DeviceNode *node;   /* the CON device node */
} SystemConsole;

static int name_is(BSTR name,const char *word)
{
    const char *a=name?(const char *)AROS_BSTR_ADDR(name):NULL;
    if(!a) return 0;
    while(*word) if(*a++!=*word++) return 0;
    return !*a;
}

/* Where the system console handler is: in the CON node, or, while
 * LimpetCON has put this launcher there, in what LimpetCON saved. */
static int system_console(SystemConsole *out,struct ExecBase *SysBase,struct DosLibrary *DOSBase)
{
    LimpetConState *state;
    struct DosList *list;
    struct DeviceNode *con;
    BPTR limpet_seg=0,saved_seg=0;
    ULONG saved_stack=0; LONG saved_priority=0; int active=0;
    Forbid();
    state=(LimpetConState *)FindPort((CONST_STRPTR)LIMPETCON_PORT);
    if(state && state->magic==LIMPETCON_MAGIC) {
        active=state->active; limpet_seg=state->limpet_seg;
        saved_seg=state->seglist; saved_stack=state->stack; saved_priority=state->priority;
    }
    Permit();
    list=LockDosList(LDF_DEVICES|LDF_READ);
    con=(struct DeviceNode *)FindDosEntry(list,(CONST_STRPTR)"CON",LDF_DEVICES);
    out->node=con; out->seglist=0;
    if(con) {
        if(active && limpet_seg && con->dn_SegList==limpet_seg) {
            out->seglist=saved_seg; out->stack=saved_stack; out->priority=saved_priority;
        } else {
            out->seglist=con->dn_SegList; out->stack=con->dn_StackSize; out->priority=con->dn_Priority;
        }
    }
    UnLockDosList(LDF_DEVICES|LDF_READ);
    /* A CON node never used since the boot has no segment yet; DOS finds
       it among the resident ones, as RunHandler does. */
    if(con && !out->seglist && con->dn_Handler) {
        struct Segment *segment;
        Forbid();
        segment=FindSegment((CONST_STRPTR)AROS_BSTR_ADDR(con->dn_Handler),NULL,TRUE);
        if(segment) out->seglist=segment->seg_Seg;
        Permit();
    }
    return out->seglist?0:-1;
}

/* Hands this open to the system console handler: starts it as DOS would
 * (rom/dos/runhandler.c), gives it the startup packet, and passes packets
 * on while DOS sends them here.
 *
 * DOS regards this process as the handler it started, and sends the open
 * (ACTION_FIND*) to this process port. A system whose handler names its
 * own port in the handle (mainline's CON: sets fh_Type) then talks to it
 * directly, and this process ends after that first packet. Elsewhere
 * (ABIv11 sets fh_Type to the port the open went to) every packet of
 * the handle comes here for as long as it is open: opens and closes are
 * counted on their way back, everything else is passed on untouched.
 *
 * Returns 0 when the startup packet was taken over (answered by the
 * system handler), -1 when nothing was done with it. */
#define XPROXY_SLOTS 8
static int fall_back(struct Message *startup_message,struct ExecBase *SysBase,struct DosLibrary *DOSBase)
{
    struct Process *self=(struct Process *)FindTask(NULL),*console;
    struct DosPacket *startup=(struct DosPacket *)startup_message->mn_Node.ln_Name;
    struct { struct DosPacket *packet; struct MsgPort *port; } slots[XPROXY_SLOTS];
    SystemConsole system;
    struct MsgPort *replies,*target,*origin;
    struct Message *m;
    SIPTR arg2,arg3;
    unsigned pending=0,i;
    long handles=0;
    int first=1,ok;
    if(system_console(&system,SysBase,DOSBase)) return -1;
    replies=CreateMsgPort();
    if(!replies) return -1;
    console=CreateNewProcTags(
        NP_Entry,(IPTR)((UBYTE *)BADDR(system.seglist)+sizeof(IPTR)),
        NP_Seglist,(IPTR)system.seglist,
        NP_FreeSeglist,(IPTR)FALSE,
        NP_Name,(IPTR)"CON",
        NP_StackSize,system.stack,
        NP_Priority,system.priority,
        TAG_DONE);
    if(!console) { DeleteMsgPort(replies); return -1; }
    target=&console->pr_MsgPort;
    /* The startup packet as the CON node would have caused it: that
       handler reads its mode (CON or RAW) from the node and the startup
       value. The name stays the caller's. */
    origin=startup->dp_Port; arg2=startup->dp_Arg2; arg3=startup->dp_Arg3;
    startup->dp_Arg2=(SIPTR)system.node->dn_Startup;
    startup->dp_Arg3=(SIPTR)MKBADDR(system.node);
    startup->dp_Port=replies;
    PutMsg(target,startup_message);
    WaitPort(replies); GetMsg(replies);
    ok=startup->dp_Res1!=DOSFALSE;
    startup->dp_Arg2=arg2; startup->dp_Arg3=arg3;
    /* DOS takes this process's port for the handler when the startup
       succeeded; from here on it must be served. */
    PutMsg(origin,startup_message);
    for(i=0;i<XPROXY_SLOTS;++i) slots[i].packet=NULL;
    while(ok && (first || handles>0 || pending)) {
        Wait((1UL<<replies->mp_SigBit)|(1UL<<self->pr_MsgPort.mp_SigBit));
        while((m=GetMsg(replies))) {
            struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
            for(i=0;i<XPROXY_SLOTS && slots[i].packet!=p;++i) {}
            if(i==XPROXY_SLOTS) continue;   /* not one of ours: cannot be */
            if(p->dp_Type==ACTION_END) { if(p->dp_Res1!=DOSFALSE) --handles; }
            else if(p->dp_Res1!=DOSFALSE) {
                /* An open that succeeded. Read the handle before the
                   answer goes back: its owner may free it then. */
                struct FileHandle *handle=BADDR((BPTR)p->dp_Arg1);
                if(handle && (!handle->fh_Type || handle->fh_Type==&self->pr_MsgPort)) ++handles;
            }
            first=0; --pending;
            slots[i].packet=NULL;
            PutMsg(slots[i].port,m);
        }
        while(pending<XPROXY_SLOTS && (m=GetMsg(&self->pr_MsgPort))) {
            struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
            if(p->dp_Type==ACTION_FINDINPUT || p->dp_Type==ACTION_FINDOUTPUT ||
               p->dp_Type==ACTION_FINDUPDATE || p->dp_Type==ACTION_END) {
                for(i=0;slots[i].packet;++i) {}
                slots[i].packet=p; slots[i].port=p->dp_Port;
                p->dp_Port=replies; ++pending;
            }
            PutMsg(target,m);
        }
    }
    DeleteMsgPort(replies);
    return 0;
}

__startup AROS_PROCH(XTermHandler,argstr,argsize,SysBase)
{
    AROS_PROCFUNC_INIT
    struct DosLibrary *DOSBase;
    struct Process *self=(struct Process *)FindTask(NULL);
    struct Message *message;
    struct DeviceNode *node;
    XHandlerContext context;
    APTR old_data=self->pr_Task.tc_UserData;
    BPTR worker=0,dir=0,old_dir=0;
    LONG error=ERROR_OBJECT_NOT_FOUND,result=RETURN_FAIL;
    char filename[1024];
    UBYTE arguments[]=XHANDLER_ARGUMENT;
    CONST_STRPTR source;
    STRPTR part;
    unsigned n=0;
    int replaces_con;
    (void)argstr; (void)argsize;
    WaitPort(&self->pr_MsgPort); message=GetMsg(&self->pr_MsgPort);
    context.magic=XHANDLER_MAGIC;
    context.startup=(struct DosPacket *)message->mn_Node.ln_Name;
    context.replied=0;
    DOSBase=(struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library",0);
    /* dos.library necessarily exists when DOS launches a handler. If an open
     * still fails, use the packet's original reply port without DOS helpers. */
    if(!DOSBase) {
        struct DosPacket *p=context.startup;
        struct MsgPort *reply=p->dp_Port;
        p->dp_Res1=DOSFALSE; p->dp_Res2=ERROR_INVALID_RESIDENT_LIBRARY;
        p->dp_Port=&self->pr_MsgPort; PutMsg(reply,p->dp_Link);
        return RETURN_FAIL;
    }
    node=(struct DeviceNode *)BADDR(context.startup->dp_Arg3);
    /* Started for CON: itself (LimpetCON ON): whatever this console cannot
       do, the system's console does instead. */
    replaces_con=node && name_is(node->dn_Name,"CON");
    /* WINDOW hands the console a window of the caller's. Limpet draws its
       own, so the system console serves that open, without loading the
       worker first. */
    if(context.startup->dp_Arg1 &&
       console_name_has_window((const char *)AROS_BSTR_ADDR((BSTR)context.startup->dp_Arg1)) &&
       !fall_back(message,SysBase,DOSBase)) {
        CloseLibrary((struct Library *)DOSBase);
        return RETURN_OK;
    }
    if(!node || !node->dn_Handler) goto done;
    source=AROS_BSTR_ADDR(node->dn_Handler);
    while(source[n] && n<sizeof filename-1) { filename[n]=source[n]; ++n; }
    if(source[n]) { error=ERROR_LINE_TOO_LONG; goto done; }
    filename[n]=0;
    /* A bare resident name / L: search would make sibling resolution ambiguous. */
    for(n=0;filename[n] && filename[n]!=':';++n) {}
    if(!filename[n]) goto done;
    part=FilePart((CONST_STRPTR)filename);
    *part=0; dir=Lock((CONST_STRPTR)filename,SHARED_LOCK);
    if(!dir) { error=IoErr(); goto done; }
    if(!AddPart((STRPTR)filename,(CONST_STRPTR)XHANDLER_WORKER,sizeof filename)) {
        error=IoErr(); goto done;
    }
    worker=LoadSeg((CONST_STRPTR)filename);
    if(!worker) { error=IoErr(); goto done; }
    old_dir=SetProgramDir(dir);
    self->pr_Task.tc_UserData=&context;
    result=RunCommand(worker,524288,arguments,
                      sizeof(XHANDLER_ARGUMENT)-1);
    error=IoErr();
    self->pr_Task.tc_UserData=old_data;
    SetProgramDir(old_dir);
done:
    if(worker) UnLoadSeg(worker);
    if(dir) UnLock(dir);
    /* As CON:, a console that did not come up (no worker, no fonts, no
       memory, a name it refuses) is the system's to open or to refuse. */
    if(!context.replied && replaces_con && !fall_back(message,SysBase,DOSBase)) {
        CloseLibrary((struct Library *)DOSBase);
        return RETURN_OK;
    }
    if(!context.replied) ReplyPkt(context.startup,DOSFALSE,error?error:ERROR_OBJECT_WRONG_TYPE);
    CloseLibrary((struct Library *)DOSBase);
    return result;
    AROS_PROCFUNC_EXIT
}
