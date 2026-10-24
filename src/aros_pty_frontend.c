#include "aros_pty_frontend.h"
#include "aros_tty_packets.h"
#include "aros_tty_signal.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/stdio.h>
#include <string.h>
#ifdef XTERM_PTY_DIAG
/* Diagnostic build: log handle state around each child to RAM:pty-diag.txt,
   and fail SetVBuf on handle N when XTERM_PTY_DIAG_FAIL_SETVBUF=N. */
#include <dos/var.h>
#include <stdio.h>
#include "pty_fh_diag.h"
#define PTY_DIAG_LOG "RAM:pty-diag.txt"
#endif

static ArosTtyPackets endpoint;
static ArosSession *owner;
static struct FileHandle *master,*slave;
static struct MsgPort *local_reply;
static struct DosPacket *local_packet;
static BPTR segment;
static int initialized;
static int shell_mode;
static ArosTtySignal break_route;
static struct Task *break_target;

/* ACTION_CHANGE_SIGNAL: who gets SIGBREAKF_CTRL_C when Ctrl-C is typed.
   The break gate may only bind a task known to be alive until it is bound
   (aros_tty_signal.h), so a new target is accepted when it is the waiting
   sender itself (CliInitNewcli names its own process), or when the sender
   is the current target handing the break to another process it waits for
   (SystemTagList for a synchronous child, then back). Anything else is
   refused rather than trusted as a cached pointer. Binding clears ISIG in
   the PTY configuration, so the flags in force before are put back; the
   first binding turns on ISIG with Ctrl-C as the interrupt character. */
static int break_enabled;
static unsigned break_flags,break_intr;

/* Closing a route clears ISIG/NOFLSH and intr in the PTY configuration
   (aros_tty_signal.c), so they are saved first and put back when the route
   is reopened for the next target: a raw-mode program keeps them off. */
static void break_route_close(void)
{
    if(!break_target) return;
    break_flags=endpoint.service.pair.config.local_flags&(TTY_ISIG|TTY_NOFLSH);
    break_intr=endpoint.service.pair.config.intr;
    aros_tty_signal_close(&break_route);break_target=NULL;
}
static TtyStatus break_route_open(struct Task *target)
{
    TtyConfig c;TtyStatus status=aros_tty_signal_open(&break_route,&endpoint.service,target);
    if(status!=TTY_OK) return status;
    break_target=target;
    c=endpoint.service.pair.config;
    c.local_flags=(c.local_flags&~(TTY_ISIG|TTY_NOFLSH))|break_flags;c.intr=break_intr;
    return tty_pair_configure(&endpoint.service.pair,&c);
}
static TtyHandle input_handle;
static TtyStatus change_signal_decide(struct Task *sender,struct Task *target,struct Task **old,TtyHandle handle);
static TtyStatus change_signal(void *context,struct Task *sender,struct Task *target,struct Task **old,TtyHandle handle)
{
    TtyStatus status;
#ifdef XTERM_PTY_DIAG
    struct Task *before=break_target;
#endif
    (void)context;
    status=change_signal_decide(sender,target,old,handle);
#ifdef XTERM_PTY_DIAG
    {
        char line[200];
        snprintf(line,sizeof line,"break %s handle=%lu sender=%p(%s) target=%p %p->%p status=%d\n",
                 handle?"explicit":"read",(unsigned long)handle,(void *)sender,
                 sender&&sender->tc_Node.ln_Name?sender->tc_Node.ln_Name:"?",(void *)target,
                 (void *)before,(void *)break_target,(int)status);
        pty_fh_diag_write(PTY_DIAG_LOG,line);
    }
#endif
    return status;
}
static TtyStatus change_signal_decide(struct Task *sender,struct Task *target,struct Task **old,TtyHandle handle)
{
    int follow=!handle;
    /* The gate forgets a task that has exited; so do we, or a new task at
       the same address would look bound while nothing is. */
    if(break_target && !aros_break_ready(&break_route.gate)) break_route_close();
    *old=break_target;
    /* The implicit READ/WRITE move needs a route that an explicit
       ACTION_CHANGE_SIGNAL established (the Shell's CliInit does). Without
       one, e.g. in --command mode, a raw-mode program's read must not turn
       ISIG on. */
    if(follow && !break_enabled) return TTY_OK;
    /* An explicit request counts only for the session's input handle: the
       Shell's CliInit and a synchronous SystemTagList name it. A Run job's
       CliInit sends it for its duplicated output and error, and must not
       take Ctrl-C from the foreground command (2026-09-28). */
    if(!follow && handle!=input_handle) return TTY_OK;
    if(!target || target==break_target) return TTY_OK;
    /* Another task only from the current target; never a remembered or
       returned pointer (SystemTagList sends the old target back later). */
    if(target!=sender && (!break_target || sender!=break_target)) return TTY_INVALID;
    if(!break_enabled) { break_flags=TTY_ISIG;break_intr=3+1;break_enabled=1; }
    break_route_close();
    return break_route_open(target);
}

/* Start an AROS Shell on this child's PTY handles the way NewShell does:
   SystemTagList with SYS_UserShell and no command is CLI_NEWCLI, so DOS
   creates the Shell process and sends it its startup packet (a Shell
   segment started with RunCommand has none and fails in PutMsg). All three
   handles are given explicitly: without SYS_Output, CliInitNewcli would
   Open("*") by name, which this endpoint does not serve. SystemTagList
   waits for the startup reply, during which the Shell sends packets to the
   PTY; that is why this runs here and not in the GUI process.

   An asynchronous Shell owns its handles and closes them at EndCLI. So in
   this mode the child is created with NP_Close* FALSE (dos.library keeps
   its own copy of pr_CIS/pr_COS/pr_CES from before the entry point,
   rom/dos/createnewproc.c); the child closes them only if the Shell did
   not start.

   The wait is tied to the Shell process itself: SystemTagList passes
   NP_NotifyOnDeath on to CreateNewProc (it is not in its filter list), so
   the Shell becomes a death-notified child of this process, which creates
   no other children, and ChildWait(0) returns exactly when it has gone. A
   CLI number was used before, but a number can be given to another Shell
   between two checks. ETask has no exit code for it (et_Result1 is never
   set in rom/), so the result here is only whether the Shell ran. */
static LONG run_shell(void)
{
    LONG rc;IPTR child;
    rc=SystemTags((CONST_STRPTR)"",
        SYS_Input,(IPTR)Input(),SYS_Output,(IPTR)Output(),SYS_Error,(IPTR)ErrorOutput(),
        /* SYS_Background defaults to TRUE, which makes CLI_ASYSTEM: a
           System shell with no prompt that ends after one command. */
        SYS_Asynch,TRUE,SYS_Background,FALSE,SYS_UserShell,TRUE,
        NP_NotifyOnDeath,TRUE,NP_StackSize,262144,TAG_DONE);
    if(rc==-1) {
        LONG error=IoErr();
        Close(Input());Close(Output());Close(ErrorOutput());
        SetIoErr(error);return RETURN_FAIL;
    }
    child=ChildWait(0);
    /* CHILD_* codes are small integers; anything else is the ETask. */
    if(child<=CHILD_EXITED) return RETURN_FAIL;
    ChildFree(((struct ETask *)child)->et_UniqueID);
    return RETURN_OK;
}

static void child_entry(void)
{
    ArosSession *s=FindTask(NULL)->tc_UserData;
    LONG out_ok,err_ok;
    Wait(SIGBREAKF_CTRL_F);
#ifdef XTERM_PTY_DIAG
    pty_fh_diag(PTY_DIAG_LOG,"before-run out",Output());
    pty_fh_diag(PTY_DIAG_LOG,"before-run err",ErrorOutput());
#endif
    if(shell_mode) { s->result=run_shell();return; }
    s->result=RunCommand(segment,262144,(STRPTR)"\n",1);
#ifdef XTERM_PTY_DIAG
    pty_fh_diag(PTY_DIAG_LOG,"after-run out",Output());
    pty_fh_diag(PTY_DIAG_LOG,"after-run err",ErrorOutput());
#endif
    /* As the Shell does after every command (workbench/c/Shell/Shell.c):
       RunCommand flushes only Input(), and a last line without a newline
       stays in the line buffer. */
    SetIoErr(0);out_ok=Flush(Output());
#ifdef XTERM_PTY_DIAG
    {
        char line[128];
        snprintf(line,sizeof line,"run result=%ld flush out result=%ld ioerr=%ld\n",
                 (long)s->result,(long)out_ok,(long)IoErr());
        pty_fh_diag_write(PTY_DIAG_LOG,line);
    }
#endif
    SetIoErr(0);err_ok=Flush(ErrorOutput());
#ifdef XTERM_PTY_DIAG
    {
        char line[96];
        snprintf(line,sizeof line,"flush err result=%ld ioerr=%ld\n",(long)err_ok,(long)IoErr());
        pty_fh_diag_write(PTY_DIAG_LOG,line);
        pty_fh_diag(PTY_DIAG_LOG,"after-flush out",Output());
    }
#endif
    (void)out_ok;(void)err_ok;
}
/* Owner-local Close: normal Close would synchronously wait on our own port. */
static void close_local(struct FileHandle **handle)
{
    if(!*handle) return;
    local_packet->dp_Type=ACTION_END;local_packet->dp_Arg1=(*handle)->fh_Arg1;
    SendPkt(local_packet,(*handle)->fh_Type,local_reply);
    aros_tty_packets_poll(&endpoint);
    WaitPort(local_reply);GetMsg(local_reply);
    FreeDosObject(DOS_FILEHANDLE,*handle);*handle=NULL;
}
int aros_pty_active(const ArosSession *s)
{ return owner==s && s->task_id!=0; }
ULONG aros_pty_signals(const ArosSession *s)
{
    if(owner!=s || !initialized) return 0;
    return (1UL<<endpoint.ports[0]->mp_SigBit)|(1UL<<endpoint.ports[1]->mp_SigBit)|
           (1UL<<endpoint.timer_port->mp_SigBit);
}
static size_t receive_budget=8192;
void aros_pty_set_receive_budget(size_t bytes) { receive_budget=bytes<1024?1024:bytes; }
static void pump(ArosSession *s)
{
    unsigned char bytes[1024];TtyResult r;size_t n,budget=receive_budget;
    if(!initialized) return;
    aros_tty_packets_poll(&endpoint);
    if(!s->stopping && s->input.length) {
        n=XSESSION_CAPACITY-s->input.head;
        if(n>s->input.length) n=s->input.length;
        r=tty_pair_write(&endpoint.service.pair,TTY_MASTER,s->input.bytes+s->input.head,n);
        s->input.head=(s->input.head+r.count)%XSESSION_CAPACITY;s->input.length-=r.count;
    }
    aros_tty_packets_poll(&endpoint);
    /* Bounded work per GUI turn. Each read makes room in the 1 KiB PTY
       queue, and the poll after it lets a Write() blocked on that queue
       continue at once (nothing else will signal this task for it), so one
       turn can move several kilobytes of a large write without a task
       switch per kilobyte. The budget keeps keys and window events served
       while a producer writes continuously. */
    while(budget && !s->stopping) {
        n=XSESSION_CAPACITY-s->output.length;if(n>sizeof bytes) n=sizeof bytes;
        if(n>budget) n=budget;
        if(!n) break;
        r=tty_pair_read(&endpoint.service.pair,TTY_MASTER,bytes,n);
        if(!r.count) break;
        session_queue_write(&s->output,bytes,r.count);
        budget-=r.count;
        aros_tty_packets_poll(&endpoint);
    }
}
void aros_pty_poll(ArosSession *s)
{
    if(owner!=s || !initialized) return;
    if(s->columns && s->rows) {
        TtyGeometry g={TTY_PAIR_VERSION,s->rows,s->columns,s->pixel_width,s->pixel_height};
        (void)tty_pair_set_geometry(&endpoint.service.pair,&g);
    }
    endpoint.presentation_enabled=s->presentation_enabled;
    endpoint.presentation_features=s->presentation_features;
    pump(s);
    if(s->task_id && ChildStatus(s->task_id)==CHILD_EXITED) {
        ChildFree(s->task_id);s->task_id=0;s->exited=1;s->eof=1;
        s->input.head=s->input.length=0;
    }
}
size_t aros_pty_write(ArosSession *s,const unsigned char *bytes,size_t n)
{
    size_t used;
    if(!aros_pty_active(s) || s->eof || n>XSESSION_CAPACITY-s->input.length) return 0;
    used=session_queue_write(&s->input,bytes,n);pump(s);return used;
}
size_t aros_pty_read(ArosSession *s,unsigned char *bytes,size_t n)
{
    size_t used=session_queue_read(&s->output,bytes,n);
    if(owner==s) pump(s);
    return used;
}
void aros_pty_stop(ArosSession *s)
{
    if(owner!=s) return;
    s->stopping=s->eof=1;s->input.length=s->output.length=0;
    /* Hanging up (window closed, F5) breaks the foreground command, as a
       hangup does elsewhere; otherwise a long command would keep this
       process alive with no window. Background jobs are not the break
       target and run on; their I/O on the stopped PTY fails. The route must
       close before its service stops (aros_tty_signal.h). */
#ifdef XTERM_PTY_DIAG
    {
        char line[120];
        /* Address only: break_target may name a task that has exited (the
           gate clears its own copy, not this one), so it is not dereferenced. */
        snprintf(line,sizeof line,"hangup break target=%p\n",(void *)break_target);
        pty_fh_diag_write(PTY_DIAG_LOG,line);
    }
#endif
    if(break_target) aros_break_send(&break_route.gate);
    aros_tty_signal_close(&break_route);break_target=NULL;break_enabled=0;input_handle=0;
    if(initialized) aros_tty_packets_stop(&endpoint);
}
void aros_pty_eof(ArosSession *s)
{
    /* A GUI close/forced end is hangup, not an injected raw VEOF byte. */
    aros_pty_stop(s);
}
int aros_pty_dispose(ArosSession *s)
{
    if(!owner) { memset(s,0,sizeof *s);return 0; }
    if(owner!=s || s->task_id) return -1;
    if(initialized) {
        close_local(&slave);close_local(&master);
        if(aros_tty_packets_dispose(&endpoint)) return -1;
    } else {
        if(master) FreeDosObject(DOS_FILEHANDLE,master);
        if(slave) FreeDosObject(DOS_FILEHANDLE,slave);
        master=slave=NULL;
    }
    if(local_packet) FreeDosObject(DOS_STDPKT,local_packet);
    if(local_reply) DeleteMsgPort(local_reply);
    if(segment) UnLoadSeg(segment);
    local_packet=NULL;local_reply=NULL;segment=0;initialized=0;owner=NULL;
    memset(&endpoint,0,sizeof endpoint);memset(s,0,sizeof *s);return 0;
}
int aros_pty_start(ArosSession *s,const char *executable)
{
    BPTR handles[3]={0,0,0};struct Process *child;unsigned i;
    LONG error=ERROR_NO_FREE_STORE;
    unsigned columns=s->columns,rows=s->rows;
    /* A NULL executable starts an AROS Shell (--shell). */
    if(owner || (executable && !*executable)) return -1;
    memset(s,0,sizeof *s);owner=s;s->columns=columns?columns:80;s->rows=rows?rows:24;
    s->console=1;s->line.raw=1;s->result=-1;
    shell_mode=!executable;
    if(!shell_mode) {
        segment=LoadSeg((CONST_STRPTR)executable);
        if(!segment) { error=IoErr();goto fail; }
    }
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    local_reply=CreateMsgPort();local_packet=AllocDosObject(DOS_STDPKT,NULL);
    if((!shell_mode && !segment) || !master || !slave || !local_reply || !local_packet) goto fail;
    if(aros_tty_packets_init(&endpoint,master,slave)) goto fail;
    initialized=1;
    endpoint.change_signal=change_signal;
    { TtyConfig c=endpoint.service.pair.config;
      c.local_flags=TTY_CANON|TTY_ECHO;c.input_flags=TTY_ICRNL;c.output_flags=TTY_OPOST|TTY_ONLCR;
      if(tty_pair_configure(&endpoint.service.pair,&c)!=TTY_OK) goto fail; }
    aros_pty_poll(s);
    for(i=0;i<3;++i) {
        struct FileHandle *h=AllocDosObject(DOS_FILEHANDLE,NULL);
        if(!h) goto fail;
        if(aros_tty_packets_duplicate(&endpoint,slave->fh_Arg1,h)!=TTY_OK) {
            FreeDosObject(DOS_FILEHANDLE,h);goto fail;
        }
        /* dos Open() gives interactive handles BUF_LINE (rom/dos/open.c).
           These are made by hand and were fully buffered: a program's
           buffered output (posixc fputs/printf end in FPuts/FWrite) that it
           did not flush never reached the terminal (2026-09-27). The session
           depends on it, so a failure stops the start instead of quietly
           bringing that back. Recorded first so fail: closes it locally. */
        handles[i]=MKBADDR(h);
        if(!i) input_handle=(TtyHandle)h->fh_Arg1;
#ifdef XTERM_PTY_DIAG
        {
            char which[8];
            if(GetVar((CONST_STRPTR)"XTERM_PTY_DIAG_FAIL_SETVBUF",(STRPTR)which,sizeof which,GVF_GLOBAL_ONLY)>0 &&
               (unsigned)(which[0]-'0')==i) {
                pty_fh_diag_write(PTY_DIAG_LOG,"forced SetVBuf failure\n");
                error=ERROR_NO_FREE_STORE;goto fail;
            }
        }
        /* XTERM_PTY_DIAG_SKIP_SETVBUF: measure the failure path without it. */
        char skip[8];
        if(GetVar((CONST_STRPTR)"XTERM_PTY_DIAG_SKIP_SETVBUF",(STRPTR)skip,sizeof skip,GVF_GLOBAL_ONLY)<=0) {
#endif
        if(SetVBuf(handles[i],NULL,BUF_LINE,-1)!=0) {
            error=IoErr();if(!error) error=ERROR_NO_FREE_STORE;goto fail;
        }
#ifdef XTERM_PTY_DIAG
        }
#endif
        /* Output and error start in buffered write mode, as a Shell's handles
           do: a zero-length FPuts allocates the buffer and sets write mode
           without sending a packet (rom/dos/fwrite.c), so it is safe here in
           the handler's own process. stdcio.library saves a handle's fh_Flags
           when a program opens it and writes them back when it closes
           (compiler/crt/stdc/__stdio.c). On a fresh handle that cleared write
           mode under the program's unflushed output, and Flush then dropped
           it as read data with ERROR_ACTION_NOT_KNOWN (measured 2026-09-27).
           Upstream stdcio restores only the mode bits since da9bbd01b5
           (2026-09-24); older systems need this, and it is harmless after.
           Without the buffer the session would lose output, so a failure
           stops the start like a failed SetVBuf. */
        if(i && FPuts(handles[i],(CONST_STRPTR)"")!=0) {
            error=IoErr();if(!error) error=ERROR_NO_FREE_STORE;goto fail;
        }
    }
    child=CreateNewProcTags(NP_Entry,(IPTR)child_entry,NP_UserData,(IPTR)s,
        NP_Name,(IPTR)"XTerm PTY application",NP_StackSize,262144,NP_Cli,TRUE,
        NP_Input,handles[0],NP_Output,handles[1],NP_Error,handles[2],
        NP_CloseInput,!shell_mode,NP_CloseOutput,!shell_mode,NP_CloseError,!shell_mode,
        NP_NotifyOnDeath,TRUE,NP_WindowPtr,(IPTR)-1,
        /* without this, every Shell session left PROGDIR locked (drawer
           undeletable); a Shell needs no home directory */
        shell_mode?NP_HomeDir:TAG_IGNORE,(IPTR)BNULL,TAG_DONE);
    if(!child) { error=IoErr();goto fail; }
    s->task_id=GetETaskID(&child->pr_Task);
    /* Mainline CreateNewProc creates an extended task. Never unload live code
       if that invariant is violated. */
    if(!s->task_id) { for(;;) Wait(0); }
    close_local(&slave);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);return 0;
fail:
    for(i=0;i<3;++i) if(handles[i]) {
        struct FileHandle *h=BADDR(handles[i]);close_local(&h);
    }
    aros_pty_stop(s);aros_pty_dispose(s);SetIoErr(error);return -1;
}
