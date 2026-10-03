/* DOS caches this segment. Keep ALL mutable state on the invocation's stack;
 * no CRT, autoinit, mutable globals or shared worker segment. */
#include "../src/aros_handler.h"
#include <aros/asmcall.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/dos.h>

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
    if(!AddPart((STRPTR)filename,(CONST_STRPTR)"aros-xterm-worker",sizeof filename)) {
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
    if(!context.replied) ReplyPkt(context.startup,DOSFALSE,error?error:ERROR_OBJECT_WRONG_TYPE);
    CloseLibrary((struct Library *)DOSBase);
    return result;
    AROS_PROCFUNC_EXIT
}
