#include "aros_session.h"
#include "xterm_presentation.h"
#include "session_command.h"
#include "xterm_status.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/stdio.h>
#include <devices/timer.h>
#include <string.h>

#ifdef XTERM_SESSION_TESTING
int aros_session_test_fail_shell_launch;
#endif

static void worker(void)
{
    ArosSession *s=(ArosSession *)FindTask(NULL)->tc_UserData;
    struct Process *me=(struct Process *)FindTask(NULL);
    BPTR startup;
    Wait(SIGBREAKF_CTRL_F); /* Parent records the child id before it can exit. */
    startup=s->startup_input;
#ifdef XTERM_SESSION_TESTING
    if(s->shell && aros_session_test_fail_shell_launch) s->result=-1;
    else
#endif
    s->result=SystemTags((CONST_STRPTR)s->command,SYS_Input,Input(),
        SYS_Output,Output(),SYS_Error,me->pr_CES,SYS_Asynch,FALSE,
        SYS_ScriptInput,startup,SYS_UserShell,s->shell?TRUE:FALSE,
        SYS_Background,s->shell?FALSE:TRUE,
        NP_StackSize,262144,TAG_DONE);
    /* The packet endpoint records actual Close, not the final command result
     * (which may legitimately be -1). Reclaim an unconsumed startup on failure. */
    if(s->startup_input) Close(s->startup_input);
}

static void cancel_timer(ArosSession *s)
{
    if(!s->timer_active) return;
    if(!CheckIO((struct IORequest *)s->timer)) AbortIO((struct IORequest *)s->timer);
    WaitIO((struct IORequest *)s->timer);
    s->timer_active=0;
}

static int start_timer(ArosSession *s,LONG usec)
{
    if(!s->timer) {
        s->timer_port=CreateMsgPort();
        if(!s->timer_port) return -1;
        s->timer=(struct timerequest *)CreateIORequest(s->timer_port,sizeof *s->timer);
        if(!s->timer) { DeleteMsgPort(s->timer_port); s->timer_port=NULL; return -1; }
        if(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)s->timer,0)) {
            DeleteIORequest((struct IORequest *)s->timer); s->timer=NULL;
            DeleteMsgPort(s->timer_port); s->timer_port=NULL; return -1;
        }
    }
    s->timer->tr_node.io_Command=TR_ADDREQUEST;
    s->timer->tr_time.tv_secs=usec/1000000;
    s->timer->tr_time.tv_micro=usec%1000000;
    s->timer_active=1;
    SendIO((struct IORequest *)s->timer);
    return 0;
}

static void service(ArosSession *s)
{
    struct DosPacket *p=s->pending_read;
    if(s->console && s->eof && s->line.length &&
       XSESSION_CAPACITY-s->input.length>=s->line.length) {
        session_queue_write(&s->input,s->line.line,s->line.length);
        s->line.length=0;
    }
    if(s->pending_wait) {
        int expired=s->timer_active && CheckIO((struct IORequest *)s->timer)!=NULL;
        int ready=session_queue_wait(&s->input,s->eof,expired);
        if(ready>=0) {
            struct DosPacket *waiter=s->pending_wait;
            s->pending_wait=NULL;
            cancel_timer(s);
            ReplyPkt(waiter,ready?DOSTRUE:DOSFALSE,0);
        }
    }
    if(p && (s->input.length || s->eof)) {
        size_t n=session_queue_read(&s->input,(unsigned char *)p->dp_Arg2,(size_t)p->dp_Arg3);
        s->pending_read=NULL;
        ReplyPkt(p,n,0);
    }
    p=s->pending_write;
    if(p) {
        if(s->stopping) {
            s->pending_write=NULL; s->write_offset=0;
            ReplyPkt(p,-1,ERROR_BREAK);
        } else {
            if(s->console && !s->line.raw) {
                const unsigned char *bytes=(const unsigned char *)p->dp_Arg2;
                while(s->write_offset<(size_t)p->dp_Arg3) {
                    unsigned char c=bytes[s->write_offset];
                    size_t needed=c=='\n'?2:1;
                    if(XSESSION_CAPACITY-s->output.length<needed) break;
                    if(c=='\n') session_queue_write(&s->output,(const unsigned char *)"\r",1);
                    session_queue_write(&s->output,&c,1); ++s->write_offset;
                }
            } else s->write_offset+=session_queue_write(&s->output,
                    (const unsigned char *)p->dp_Arg2+s->write_offset,
                    (size_t)p->dp_Arg3-s->write_offset);
            if(s->write_offset==(size_t)p->dp_Arg3) {
                s->pending_write=NULL; s->write_offset=0;
                ReplyPkt(p,p->dp_Arg3,0);
            }
        }
    }
}

static int lock_index(ArosSession *s,BPTR lock)
{
    unsigned i;
    for(i=0;i<16;++i)
        if((s->lock_mask&(1U<<i)) && MKBADDR(&s->locks[i])==lock) return (int)i;
    return -1;
}

int aros_session_peer_command(char *command,size_t capacity)
{
    char path[240];
    if(!NameFromLock(GetProgramDir(),(STRPTR)path,sizeof path) ||
       !AddPart((STRPTR)path,(CONST_STRPTR)"session-peer",sizeof path)) return -1;
    return session_quote_path(path,command,capacity);
}

static int start_session(ArosSession *s,const char *command,int console,int shell,BPTR startup)
{
    struct FileHandle *fh[3]={NULL,NULL,NULL};
    struct FileHandle *script=NULL;
    struct Process *child;
    unsigned i;
    if(!s || s->port || s->task_id || !command || (!*command && !shell) ||
       strlen(command)>=sizeof s->command) return -1;
    memset(s,0,sizeof *s);
    s->console=console; s->shell=shell;
    s->startup_file=startup;
    strcpy(s->command,command); s->result=-1;
    s->port=CreateMsgPort();
    if(!s->port) return -1;
    if(startup) {
        script=AllocDosObject(DOS_FILEHANDLE,NULL);
        if(!script) goto fail;
        script->fh_Type=s->port; script->fh_Arg1=4;
        SetVBuf(MKBADDR(script),NULL,BUF_NONE,-1);
        s->startup_input=MKBADDR(script);
    }
    if(console) aros_break_open(&s->breaker);
    for(i=0;i<3;++i) {
        fh[i]=AllocDosObject(DOS_FILEHANDLE,NULL);
        if(!fh[i]) goto fail;
        fh[i]->fh_Type=s->port; fh[i]->fh_Arg1=i+1;
        fh[i]->fh_Interactive=console?DOSTRUE:DOSFALSE;
        SetVBuf(MKBADDR(fh[i]),NULL,BUF_NONE,-1);
    }
    child=CreateNewProcTags(NP_Entry,(IPTR)worker,NP_UserData,(IPTR)s,
        NP_Name,(IPTR)"XTerm foreground session",NP_StackSize,262144,
        NP_Input,MKBADDR(fh[0]),NP_Output,MKBADDR(fh[1]),NP_Error,MKBADDR(fh[2]),
        NP_CloseInput,TRUE,NP_CloseOutput,TRUE,NP_CloseError,TRUE,
        NP_NotifyOnDeath,TRUE,NP_WindowPtr,(IPTR)-1,
        /* without this, every session left PROGDIR locked (drawer undeletable);
           the worker and its Shell need no home directory */
        NP_HomeDir,(IPTR)BNULL,TAG_DONE);
    if(!child) goto fail;
    s->handles=7;
    s->references[0]=s->references[1]=s->references[2]=1;
    s->task_id=GetETaskID(&child->pr_Task);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    return 0;
fail:
    if(script) FreeDosObject(DOS_FILEHANDLE,script);
    s->startup_input=BNULL;
    aros_break_close(&s->breaker);
    for(i=0;i<3;++i) if(fh[i]) FreeDosObject(DOS_FILEHANDLE,fh[i]);
    DeleteMsgPort(s->port); s->port=NULL;
    return -1;
}
int aros_session_start(ArosSession *s,const char *command)
{ return start_session(s,command,0,0,BNULL); }
int aros_session_start_console(ArosSession *s,const char *command)
{ return start_session(s,command,1,0,BNULL); }
int aros_session_start_shell(ArosSession *s)
{ return aros_session_start_shell_profile(s,NULL); }
int aros_session_start_endpoint(ArosSession *s)
{
    if(!s || s->port || s->task_id) { SetIoErr(ERROR_OBJECT_IN_USE); return -1; }
    memset(s,0,sizeof *s);
#ifdef XTERM_PROCESS_PORT_CLIENTS
    s->port=&((struct Process *)FindTask(NULL))->pr_MsgPort;
    s->borrowed_port=1;
#else
    s->port=CreateMsgPort();
    if(!s->port) { SetIoErr(ERROR_NO_FREE_STORE); return -1; }
#endif
    s->external=s->console=1;
    aros_break_open(&s->breaker);
    return 0;
}
int aros_session_active(const ArosSession *s)
{
    return s->task_id || (s->external && s->port &&
        ((!s->opened && !s->stopping) || s->handles || s->lock_mask ||
         s->pending_read || s->pending_write || s->pending_wait ||
         s->port->mp_MsgList.lh_Head->ln_Succ));
}
int aros_session_start_shell_profile(ArosSession *s,const char *profile)
{
    BPTR input=BNULL;
    LONG error;
    if(!s || s->port || s->task_id) { SetIoErr(ERROR_OBJECT_IN_USE); return -1; }
    if(!profile || *profile) {
        input=Open((CONST_STRPTR)(profile?profile:"S:Shell-Startup"),MODE_OLDFILE);
        if(!input) {
            if(profile || IoErr()!=ERROR_OBJECT_NOT_FOUND) return -1;
        } else {
            struct FileInfoBlock *fib=AllocDosObject(DOS_FIB,NULL);
            int examined=fib && ExamineFH(input,fib);
            int regular=examined && fib->fib_DirEntryType<0;
            error=!fib?ERROR_NO_FREE_STORE:!examined?IoErr():ERROR_OBJECT_WRONG_TYPE;
            if(fib) FreeDosObject(DOS_FIB,fib);
            if(!regular) { Close(input); SetIoErr(error); return -1; }
        }
    }
    if(!start_session(s,"",1,1,input)) return 0;
    error=IoErr();
    if(input) Close(input);
    s->startup_input=BNULL;
    s->startup_file=BNULL;
    SetIoErr(error); return -1;
}

ULONG aros_session_signals(const ArosSession *s)
{ return (s->port?(1UL<<s->port->mp_SigBit):0) |
    (s->timer_active?(1UL<<s->timer_port->mp_SigBit):0); }

void aros_session_poll(ArosSession *s)
{
    struct Message *m;
    unsigned budget=64;
    if(!s->port) return;
    service(s);
    while(budget-- && (m=GetMsg(s->port))) {
        struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
        IPTR id=p->dp_Arg1;
        /* Private read-only script proxy. It observes ownership transfer
         * through ACTION_END even when the completed Shell returns -1. */
        if(id==4 && (p->dp_Type==ACTION_READ || p->dp_Type==ACTION_SEEK ||
                     p->dp_Type==ACTION_EXAMINE_FH || p->dp_Type==ACTION_END)) {
            LONG result=-1,error=ERROR_INVALID_LOCK;
            if(s->startup_file) {
                if(p->dp_Type==ACTION_READ)
                    result=Read(s->startup_file,(APTR)p->dp_Arg2,p->dp_Arg3);
                else if(p->dp_Type==ACTION_SEEK)
                    result=Seek(s->startup_file,p->dp_Arg2,p->dp_Arg3);
                else if(p->dp_Type==ACTION_EXAMINE_FH)
                    result=ExamineFH(s->startup_file,BADDR((BPTR)p->dp_Arg2));
                else {
                    result=Close(s->startup_file);
                    s->startup_file=s->startup_input=BNULL;
                    ++s->startup_closes;
                }
                error=IoErr();
            }
            ReplyPkt(p,result,error); continue;
        }
        switch(p->dp_Type) {
        case XTY_ACTION_CONTROL: {
            XtyControl in;
            if(id<1 || id>3 || !(s->handles&(1UL<<(id-1))))
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else if(p->dp_Arg4!=XTY_PACKET_MAGIC || p->dp_Arg5 ||
                    !p->dp_Arg2 || p->dp_Arg3!=sizeof in)
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else {
                memcpy(&in,(void *)p->dp_Arg2,sizeof in);
                if(!s->console || !s->presentation_enabled || s->stopping ||
                   in.operation!=XTY_GET_PRESENTATION)
                    ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
                else if(!xterm_presentation_reply((void *)p->dp_Arg2,sizeof in,s->rows,s->columns,
                        s->amiga_pens?XTY_COLOR_PENS:0))
                    ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
                else ReplyPkt(p,DOSTRUE,0);
            }
            break;
        }
        case XTY_ACTION_CANCEL:
            /* The read a client still has out, taken back by its identity
             * (tty_control.h): the read is answered first, with BREAK, then
             * this packet, both to the same port, so a client that waits
             * for this answer has the read's as well. A read already
             * answered is not found; its answer is already on its way. */
            if(id<1 || id>3 || !(s->handles&(1UL<<(id-1))))
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else if(!p->dp_Arg2 || p->dp_Arg3 || p->dp_Arg4!=XTY_PACKET_MAGIC || p->dp_Arg5)
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else if(s->pending_read && (SIPTR)s->pending_read==p->dp_Arg2 &&
                    s->pending_read->dp_Arg1==p->dp_Arg1 && s->pending_read->dp_Port==p->dp_Port) {
                struct DosPacket *read=s->pending_read;
                s->pending_read=NULL;
                ReplyPkt(read,-1,ERROR_BREAK);
                ReplyPkt(p,DOSTRUE,0);
            } else ReplyPkt(p,DOSFALSE,ERROR_OBJECT_NOT_FOUND);
            break;
        case ACTION_FINDINPUT:
        case ACTION_FINDOUTPUT:
        case ACTION_FINDUPDATE: {
            struct FileHandle *fh=BADDR((BPTR)p->dp_Arg1);
            unsigned stream=p->dp_Type==ACTION_FINDOUTPUT?1:0;
            if(s->stopping || (s->external && s->exited)) {
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE); break;
            }
            if(!s->console || !fh) { ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN); break; }
            if(s->references[stream]>=32) { ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE); break; }
            fh->fh_Type=s->port; fh->fh_Arg1=stream+1; fh->fh_Interactive=DOSTRUE;
            ++s->references[stream]; s->handles|=1UL<<stream;
            s->opened=1;
            ReplyPkt(p,DOSTRUE,0); break;
        }
        case ACTION_COPY_DIR_FH: {
            unsigned i;
            if(id<1 || id>3 || !(s->handles&(1UL<<(id-1)))) {
                ReplyPkt(p,0,ERROR_INVALID_LOCK); break;
            }
            for(i=0;i<16 && (s->lock_mask&(1U<<i));++i) {}
            if(i==16) { ReplyPkt(p,0,ERROR_NO_FREE_STORE); break; }
            memset(&s->locks[i],0,sizeof s->locks[i]);
            s->locks[i].fl_Key=id; s->locks[i].fl_Task=s->port;
            s->locks[i].fl_Access=SHARED_LOCK;
            s->lock_mask|=1U<<i;
            ReplyPkt(p,(SIPTR)MKBADDR(&s->locks[i]),0); break;
        }
        case ACTION_FH_FROM_LOCK: {
            int i=lock_index(s,(BPTR)p->dp_Arg2);
            struct FileHandle *fh=BADDR((BPTR)p->dp_Arg1);
            if(s->stopping || (s->external && s->exited)) {
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE); break;
            }
            if(i<0 || !fh) { ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK); break; }
            id=s->locks[i].fl_Key;
            if(s->references[id-1]>=32) { ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE); break; }
            fh->fh_Type=s->port; fh->fh_Arg1=id;
            fh->fh_Interactive=s->console?DOSTRUE:DOSFALSE;
            ++s->references[id-1]; s->handles|=1UL<<(id-1);
            s->lock_mask&=~(1U<<i); /* OpenFromLock consumes the lock. */
            ReplyPkt(p,DOSTRUE,0); break;
        }
        case ACTION_FREE_LOCK: {
            int i=lock_index(s,(BPTR)p->dp_Arg1);
            if(i<0) ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else { s->lock_mask&=~(1U<<i); ReplyPkt(p,DOSTRUE,0); }
            break;
        }
        case ACTION_SCREEN_MODE: {
            int was_raw=s->line.raw;
            if(!s->console) ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
            else if(console_input_mode(&s->line,(int)p->dp_Arg1))
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else {
                if(was_raw!=s->line.raw) ++s->input_generation;
                ReplyPkt(p,DOSTRUE,0);
            }
            break;
        }
        case ACTION_CHANGE_SIGNAL: {
            IPTR old=s->signal_target;
            if(!s->console) { ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN); break; }
            /* Keep the DOS value opaque: mainline callers pass both a Task
             * and a MsgPort here. Do not Signal an unvalidated pointer. */
            if(p->dp_Arg2) s->signal_target=p->dp_Arg2;
            ReplyPkt(p,DOSTRUE,old); break;
        }
        case ACTION_DISK_INFO: {
            struct InfoData *info=BADDR((BPTR)p->dp_Arg1);
            if(!s->console || !info) { ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN); break; }
            memset(info,0,sizeof *info);
            info->id_DiskType=s->line.raw?0x52415700:0x434f4e00;
            info->id_VolumeNode=(BPTR)(SIPTR)-1; /* No requester window in adapter. */
            ReplyPkt(p,DOSTRUE,0); break;
        }
        case XTERM_ACTION_QUERY: {
            XTermStatus *status=(XTermStatus *)p->dp_Arg2;
            if(id!=1 || !(s->handles&1))
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else if(!status || p->dp_Arg3!=sizeof *status ||
                    status->version!=XTERM_STATUS_VERSION || status->reserved)
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else if(s->pending_read || s->pending_wait)
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);
            else {
                status->available=(uint32_t)s->input.length;
                status->eof=s->eof?1:0;
                status->columns=s->columns; status->rows=s->rows;
                ReplyPkt(p,DOSTRUE,0);
            }
            break;
        }
        case ACTION_WAIT_CHAR:
            /* DOS sends the timeout in Arg1, not a filehandle identifier.
             * This private endpoint has one input stream and one waiter. */
            if(p->dp_Arg1<0) ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else if(s->pending_wait || s->pending_read)
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);
            else if(s->input.length) ReplyPkt(p,DOSTRUE,0);
            else if(s->eof || !p->dp_Arg1) ReplyPkt(p,DOSFALSE,0);
            else if(start_timer(s,(LONG)p->dp_Arg1))
                ReplyPkt(p,DOSFALSE,ERROR_NO_FREE_STORE);
            else s->pending_wait=p;
            break;
        case ACTION_READ:
            if(id!=1 || !(s->handles&1) || p->dp_Arg3<0 ||
               (p->dp_Arg3 && !p->dp_Arg2)) ReplyPkt(p,-1,ERROR_BAD_NUMBER);
            else if(!p->dp_Arg3) ReplyPkt(p,0,0);
            else if(s->pending_read || s->pending_wait) ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);
            else {
                if(s->console) aros_break_bind(&s->breaker,p->dp_Port->mp_SigTask);
                s->pending_read=p; service(s);
            }
            break;
        case ACTION_WRITE:
            if((id!=2 && id!=3 && !(s->console && id==1)) || !(s->handles&(1UL<<(id-1))) ||
               p->dp_Arg3<0 || (p->dp_Arg3 && !p->dp_Arg2))
                ReplyPkt(p,-1,ERROR_BAD_NUMBER);
            else if(s->stopping) ReplyPkt(p,-1,ERROR_BREAK);
            else if(!p->dp_Arg3) ReplyPkt(p,0,0);
            else if(s->pending_write) ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);
            else {
                /* Writes do not move the break target: a background job
                 * that only prints must not take Ctrl-C from the Shell. The
                 * foreground is whoever reads the console; commands run in
                 * the Shell's own process, so they are covered. */
                s->pending_write=p; s->write_offset=0; service(s);
            }
            break;
        case ACTION_END:
            if(id<1 || id>3 || !(s->handles&(1UL<<(id-1))))
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else {
                if(!--s->references[id-1]) {
                    s->handles&=~(1UL<<(id-1));
                    /* Close/reopen can occur in one GUI poll. Invalidate
                     * delayed input even when no observer sees handles==0. */
                    if(id==1) ++s->input_generation;
                    if(id==1 && !s->external) { s->eof=1; s->input.length=0; service(s); }
                }
                ReplyPkt(p,DOSTRUE,0);
            }
            break;
        case ACTION_IS_FILESYSTEM:
            ReplyPkt(p,DOSFALSE,0); break;
        case ACTION_SEEK:
            /* Zero is a valid old position, not a failure for this packet. */
            ReplyPkt(p,-1,ERROR_SEEK_ERROR); break;
        default:
            s->unsupported_action=p->dp_Type;
            ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN); break;
        }
    }
    /* GetMsg does not consume the port signal. If a budget stopped this poll,
     * explicitly preserve a wakeup for the remaining packets. */
    if(s->port->mp_MsgList.lh_Head->ln_Succ)
        Signal(FindTask(NULL),1UL<<s->port->mp_SigBit);
    if(s->task_id && ChildStatus(s->task_id)==CHILD_EXITED) {
        ChildFree(s->task_id); s->task_id=0; s->exited=1; s->eof=1;
        /* Locks are embedded leases, not DOS-owned allocations. Once the sole
         * foreground child has exited and all handles closed, reclaim any
         * abandoned lease. Exporting locks to other processes is unsupported. */
        if(!s->handles) s->lock_mask=0;
    }
    if(s->external && s->opened && !aros_session_active(s)) {
        s->exited=1; s->eof=1;
    }
}

size_t aros_session_write(ArosSession *s,const unsigned char *bytes,size_t n)
{
    size_t used;
    if(!aros_session_active(s) || s->eof || (s->external && !(s->handles&1))) return 0;
    if(s->console && !s->line.raw && n==1 && bytes[0]==3)
        return aros_session_interrupt(s)?1:0;
    if(s->console) {
        XConsoleInput saved=s->line;
        size_t in=s->input.length,out=s->output.length;
        for(used=0;used<n;++used) {
            if(!console_input_byte(&s->line,&s->input,&s->output,bytes[used])) {
                s->line=saved; s->input.length=in; s->output.length=out;
                return 0;
            }
        }
        service(s); return n;
    }
    used=session_queue_write(&s->input,bytes,n); service(s);
    return used;
}
size_t aros_session_read(ArosSession *s,unsigned char *bytes,size_t n)
{
    size_t used=session_queue_read(&s->output,bytes,n);
    service(s); return used;
}
void aros_session_eof(ArosSession *s)
{ s->eof=1; service(s); }
int aros_session_foreground(ArosSession *s,XForeground *out)
{
    memset(out,0,sizeof *out);
    if(!s->console || !aros_session_active(s)) return 0;
    return aros_break_foreground(&s->breaker,out);
}
int aros_session_interrupt(ArosSession *s)
{
    XForeground fg;
    if(!s->console || !aros_session_active(s) || s->eof || s->line.raw ||
       XSESSION_CAPACITY-s->output.length<4) return 0;
    /* Before the signal: a command it ends would look like the idle Shell. */
    aros_break_foreground(&s->breaker,&fg);
    if(!aros_break_send(&s->breaker)) return 0;
    s->line.length=0; s->line.escape=s->line.skip_lf=0;
    s->input.head=s->input.length=0;
    session_queue_write(&s->output,(const unsigned char *)"^C\r\n",4);
    /* An empty line releases a reader, including a Shell whose prompt Write
     * reached us before its Read (EOF would end the Shell). A running
     * command gets only the signal: the line would wait for the Shell's
     * next Read and print a second prompt (console_interrupt_needs_line).
     * Raw clients receive the Ctrl-C byte instead of entering this path. */
    if(console_interrupt_needs_line(s->pending_read!=NULL,fg.known,fg.running))
        session_queue_write(&s->input,(const unsigned char *)"\n",1);
    service(s); return 1;
}
void aros_session_stop(ArosSession *s)
{
    s->stopping=1; s->eof=1;
    s->input.length=0; s->output.length=0;
    s->line.length=0;
    service(s);
}
int aros_session_dispose(ArosSession *s)
{
    if(aros_session_active(s) || s->task_id || s->handles || s->startup_input || s->startup_file ||
       s->lock_mask || s->pending_read || s->pending_write || s->pending_wait) return -1;
    if(s->port && s->port->mp_MsgList.lh_Head->ln_Succ) return -1;
    cancel_timer(s);
    aros_break_close(&s->breaker);
    if(s->timer) {
        CloseDevice((struct IORequest *)s->timer);
        DeleteIORequest((struct IORequest *)s->timer);
        DeleteMsgPort(s->timer_port);
    }
#ifdef XTERM_PROCESS_PORT_CLIENTS
    if(s->port && !s->borrowed_port) DeleteMsgPort(s->port);
#else
    if(s->port) DeleteMsgPort(s->port);
#endif
    memset(s,0,sizeof *s);
    return 0;
}
