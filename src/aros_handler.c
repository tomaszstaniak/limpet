#include "aros_handler.h"
#include <aros/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
/* This is a DOS worker, not a WB program or a CLI command. */
int __nowbsupport=1;
int __nostdiowin=1;
int __nocommandline=1;

XHandlerContext *aros_handler_context(void)
{
    struct Process *self=(struct Process *)FindTask(NULL);
    XHandlerContext *c;
    if(self->pr_CLI || !__argstr || strcmp((const char *)__argstr,XHANDLER_ARGUMENT))
        return NULL;
    c=self->pr_Task.tc_UserData;
    return c && c->magic==XHANDLER_MAGIC && !c->replied && c->startup?c:NULL;
}
#ifdef XTERM_PKTWAIT_PROBE
#include <aros/asmcall.h>
/* TEST BUILDS ONLY. Counts every synchronous DOS wait in this process after
 * accept. Returning NULL makes internal_WaitPkt do its normal GetMsg/Wait,
 * so reply handling is unchanged; the probe only observes. */
static volatile unsigned long pktwaits;
AROS_UFH3(struct Message *,pktwait_probe,
    AROS_UFHA(APTR,self,A0),
    AROS_UFHA(struct MsgPort *,port,A1),
    AROS_UFHA(struct ExecBase *,SysBase,A6))
{
    AROS_USERFUNC_INIT
    (void)self; (void)port; (void)SysBase;
    ++pktwaits;
    return NULL;
    AROS_USERFUNC_EXIT
}
unsigned long aros_handler_pktwaits(void) { return pktwaits; }
void aros_handler_probe_stop(void)
{ ((struct Process *)FindTask(NULL))->pr_PktWait=NULL; }
#endif

int aros_handler_accept(XHandlerContext *c,ArosSession *s)
{
    struct Process *self=(struct Process *)FindTask(NULL);
    struct Message *m;
    struct DosPacket *first;
    const char *path,*colon;
    if(!c || !s->external || !s->port) { SetIoErr(ERROR_OBJECT_WRONG_TYPE); return -1; }
    path=c->startup->dp_Arg1?(const char *)AROS_BSTR_ADDR((BSTR)c->startup->dp_Arg1):NULL;
    colon=path?strchr(path,':'):NULL;
    /* What follows the colon (geometry, title, options) was read before the
       window opened (src/console_name.h). */
    if(!colon || colon==path) { SetIoErr(ERROR_BAD_STREAM_NAME); return -1; }
    c->replied=1;
    ReplyPkt(c->startup,DOSTRUE,0);
#ifdef XTERM_PROCESS_PORT_CLIENTS
    /* No synchronous DOS call from here on: s->port IS this process port,
     * where DoPkt would also wait for its reply (see aros_session.h). */
#else
    /* No DOS calls between this reply and initial FIND: DoPkt also uses this
     * process port. Future handle traffic is routed to s->port instead. */
#endif
    WaitPort(&self->pr_MsgPort); m=GetMsg(&self->pr_MsgPort);
    first=(struct DosPacket *)m->mn_Node.ln_Name;
    if(first->dp_Type!=ACTION_FINDINPUT && first->dp_Type!=ACTION_FINDOUTPUT &&
       first->dp_Type!=ACTION_FINDUPDATE) {
        ReplyPkt(first,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
        aros_session_stop(s); return -1;
    }
    PutMsg(s->port,m);
#ifdef XTERM_PKTWAIT_PROBE
    self->pr_PktWait=(APTR)pktwait_probe;
#endif
    aros_session_poll(s);
    return 0;
}
