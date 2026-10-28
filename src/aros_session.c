#include "console_geometry.h"
#include "aros_session.h"
#include "perf_diag.h"
#include "xterm_presentation.h"
#include "session_command.h"
#include "xterm_status.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
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
    /* Programs choose their colours from COLORTERM, as on other terminals
     * (Helix keeps to 16 colours without it). A local variable of this
     * process: the session's Shell copies it (NP_CopyVars), so does every
     * program the Shell starts, and nothing outside the window sees it. */
    SetVar((CONST_STRPTR)"COLORTERM",(CONST_STRPTR)"truecolor",-1,GVF_LOCAL_ONLY);
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

/* A waiter's timer: abort it if it runs, collect it either way. */
static void wait_stop(ArosSession *s,unsigned i)
{
    if(s->waits[i].active) {
        if(!CheckIO((struct IORequest *)s->waits[i].timer)) AbortIO((struct IORequest *)s->waits[i].timer);
        WaitIO((struct IORequest *)s->waits[i].timer);
        s->waits[i].active=0;
    }
}

/* Park a WaitForChar packet with its own timeout; -1 without memory. */
static int wait_start(ArosSession *s,struct DosPacket *p,LONG usec)
{
    unsigned i;
    if(!s->timer_port && !(s->timer_port=CreateMsgPort())) return -1;
    for(i=0;i<XSESSION_WAITERS && s->waits[i].packet;++i) {}
    if(i==XSESSION_WAITERS) return -1;
    if(!s->waits[i].timer) {
        struct timerequest *t=(struct timerequest *)CreateIORequest(s->timer_port,sizeof *t);
        if(!t) return -1;
        if(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)t,0)) {
            DeleteIORequest((struct IORequest *)t); return -1;
        }
        s->waits[i].timer=t;
    }
    s->waits[i].timer->tr_node.io_Command=TR_ADDREQUEST;
    s->waits[i].timer->tr_time.tv_secs=usec/1000000;
    s->waits[i].timer->tr_time.tv_micro=usec%1000000;
    SendIO((struct IORequest *)s->waits[i].timer);
    s->waits[i].active=1;
    s->waits[i].packet=p;
    ++s->wait_count;
    return 0;
}

/* Writes to a window that has been closed fail with this error. Not
 * ERROR_BREAK: C libraries turn that into EINTR, which callers retry (Rust's
 * write_all does, for good), so a program writing to a closed window spun at
 * full CPU instead of ending. */
#define XSESSION_CLOSED_ERROR ERROR_DEVICE_NOT_MOUNTED

#ifdef XTERM_PERF_DIAG
static uint64_t perf_write_started;
#endif

/* The next read in line becomes the one served. */
static void read_next(ArosSession *s)
{
    unsigned i;
    s->pending_read=NULL;
    if(!s->read_waiting) return;
    s->pending_read=s->read_wait[0];
    for(i=1;i<s->read_waiting;++i) s->read_wait[i-1]=s->read_wait[i];
    --s->read_waiting;
}

static void service(ArosSession *s)
{
    struct DosPacket *p=s->pending_read;
    if(s->console && s->eof && s->line.length &&
       XSESSION_CAPACITY-s->input.length>=s->line.length) {
        session_queue_write(&s->input,s->line.line,s->line.length);
        s->line.length=0;
    }
    if(s->wait_count) {
        unsigned i;
        for(i=0;i<XSESSION_WAITERS;++i) if(s->waits[i].packet) {
            int expired=s->waits[i].active && CheckIO((struct IORequest *)s->waits[i].timer)!=NULL;
            /* As CON:, a partly typed line counts as a key there. */
            int typed=s->console && !s->line.raw && s->line.length;
            int ready=typed?1:session_queue_wait(&s->input,s->eof || s->eof_once || s->stopping,expired);
            if(ready>=0) {
                struct DosPacket *waiter=s->waits[i].packet;
                wait_stop(s,i);
                s->waits[i].packet=NULL; --s->wait_count;
                ReplyPkt(waiter,ready?DOSTRUE:DOSFALSE,0);
            }
        }
    }
    /* Readers in order: each takes what is there; at the end of input
       (F5, close) every one gets 0, a Ctrl-\ ends only the first. */
    while((p=s->pending_read)) {
        if(s->input.length || s->eof) {
            size_t n=session_queue_read(&s->input,(unsigned char *)p->dp_Arg2,(size_t)p->dp_Arg3);
            read_next(s);
            ReplyPkt(p,n,0);
        } else if(s->eof_once) {
            /* Typed text went first (above); this read is the end of input. */
            s->eof_once=0; read_next(s);
            ReplyPkt(p,0,0);
        } else break;
    }
    while((p=s->pending_write)) {
        if(s->stopping) {
            s->pending_write=NULL; s->write_offset=0;
            ReplyPkt(p,-1,XSESSION_CLOSED_ERROR);
        } else {
            /* Bytes as written: the line feed mode of the screen (on for a
               console, as console.device starts; CSI 20 l clears it) adds
               the carriage return, not this queue. */
            s->write_offset+=session_queue_write(&s->output,
                    (const unsigned char *)p->dp_Arg2+s->write_offset,
                    (size_t)p->dp_Arg3-s->write_offset);
            if(s->write_offset!=(size_t)p->dp_Arg3) break;
            s->pending_write=NULL; s->write_offset=0;
            PERF_DO(uint64_t waited=PERF_NOW()-perf_write_started; perf_diag.write_wait_us+=(unsigned long)waited;
                    PERF_MAX(write_wait_max_us,waited));
            ReplyPkt(p,p->dp_Arg3,0);
        }
        /* The next writer in line, if any. */
        if(s->write_waiting) {
            unsigned i;
            s->pending_write=s->write_wait[0];
            for(i=1;i<s->write_waiting;++i) s->write_wait[i-1]=s->write_wait[i];
            --s->write_waiting;
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

static int start_session(ArosSession *s,const char *command,int console,int shell,BPTR startup,
                         LONG startup_length,const char *tail)
{
    struct FileHandle *fh[3]={NULL,NULL,NULL};
    struct FileHandle *script=NULL;
    struct Process *child;
    unsigned i;
    size_t tail_length=tail && *tail?strlen(tail)+2:0;
    if(!s || s->port || s->task_id || !command || (!*command && !shell) ||
       strlen(command)>=sizeof s->command || tail_length>=XSESSION_TAIL_MAX ||
       (tail && strpbrk(tail,"\r\n"))) return -1;
    memset(s,0,sizeof *s);
    s->console=console; s->shell=shell;
    s->startup_file=startup;
    /* The line starts on a line of its own, whatever the file's last byte. */
    if(tail_length) {
        s->startup_tail[0]='\n'; strcpy(s->startup_tail+1,tail);
        s->startup_tail[tail_length-1]='\n';
    }
    script_stream_init(&s->script,startup?startup_length:0,(long)tail_length);
    strcpy(s->command,command); s->result=-1;
    s->port=CreateMsgPort();
    if(!s->port) return -1;
    if(startup || tail_length) {
        s->script_proxy=1;
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
{ return start_session(s,command,0,0,BNULL,0,NULL); }
int aros_session_start_console(ArosSession *s,const char *command)
{ return start_session(s,command,1,0,BNULL,0,NULL); }
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
         s->pending_read || s->pending_write || s->wait_count ||
         s->port->mp_MsgList.lh_Head->ln_Succ));
}
int aros_session_start_shell_profile(ArosSession *s,const char *profile)
{ return aros_session_start_shell_run(s,profile,NULL); }
int aros_session_start_shell_run(ArosSession *s,const char *profile,const char *line)
{
    BPTR input=BNULL;
    LONG error,length=0;
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
            if(regular) length=fib->fib_Size;
            if(fib) FreeDosObject(DOS_FIB,fib);
            if(!regular) { Close(input); SetIoErr(error); return -1; }
        }
    }
    if(!start_session(s,"",1,1,input,length,line)) return 0;
    error=IoErr();
    if(input) Close(input);
    s->startup_input=BNULL;
    s->startup_file=BNULL;
    SetIoErr(error); return -1;
}

ULONG aros_session_signals(const ArosSession *s)
{ return (s->port?(1UL<<s->port->mp_SigBit):0) |
    (s->wait_count?(1UL<<s->timer_port->mp_SigBit):0); }

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
            if(s->script_proxy) {
                error=0;
                if(p->dp_Type==ACTION_READ) {
                    /* The file, then the tail: one stream (script_tail.h). */
                    LONG part=script_stream_file_part(&s->script,p->dp_Arg3);
                    result=0;
                    if(part && s->startup_file) {
                        result=Read(s->startup_file,(APTR)p->dp_Arg2,part);
                        if(result<0) error=IoErr();
                        else script_stream_file_read(&s->script,result);
                    } else if(part) script_stream_file_read(&s->script,0);
                    if(!result)
                        result=script_stream_tail_read(&s->script,s->startup_tail,
                                                       (char *)p->dp_Arg2,p->dp_Arg3);
                } else if(p->dp_Type==ACTION_SEEK) {
                    result=script_stream_seek(&s->script,p->dp_Arg2,p->dp_Arg3);
                    if(result<0) error=ERROR_SEEK_ERROR;
                    else if(s->startup_file && s->script.position<s->script.file_length &&
                            Seek(s->startup_file,s->script.position,OFFSET_BEGINNING)<0) {
                        error=IoErr(); script_stream_seek(&s->script,result,OFFSET_BEGINNING); result=-1;
                    }
                } else if(p->dp_Type==ACTION_EXAMINE_FH) {
                    struct FileInfoBlock *fib=BADDR((BPTR)p->dp_Arg2);
                    if(s->startup_file) {
                        result=ExamineFH(s->startup_file,fib);
                        if(!result) error=IoErr();
                    } else {
                        memset(fib,0,sizeof *fib);
                        fib->fib_DirEntryType=fib->fib_EntryType=ST_FILE;
                        result=DOSTRUE;
                    }
                    if(result) fib->fib_Size=s->script.file_length+s->script.tail_length;
                } else {
                    result=DOSTRUE;
                    if(s->startup_file) { result=Close(s->startup_file); error=IoErr(); }
                    s->startup_file=s->startup_input=BNULL;
                    s->script_proxy=0;
                    ++s->startup_closes;
                }
            }
            ReplyPkt(p,result,error); continue;
        }
        switch(p->dp_Type) {
        case ACTION_CONSOLE_GETGEOMETRY:
            if(id<1 || id>3 || !(s->handles&(1UL<<(id-1))) || s->stopping)
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else if(!s->console || !s->presentation_enabled)
                ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
            else if(!console_geometry_reply((void *)p->dp_Arg2,(SIPTR)p->dp_Arg3,
                        p->dp_Arg4,s->columns,s->rows,s->pixel_width,s->pixel_height))
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else ReplyPkt(p,DOSTRUE,0);
            break;
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
                   (in.operation!=XTY_GET_PRESENTATION && in.operation!=XTY_GET_IDENTITY))
                    ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
                else if(in.operation==XTY_GET_IDENTITY) {
                    if(xterm_identity_reply((void *)p->dp_Arg2,sizeof in,XTERM_IDENTITY_NAME,LIMPET_VERSION))
                        ReplyPkt(p,DOSTRUE,0);
                    else ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
                }
                else if(!xterm_presentation_reply((void *)p->dp_Arg2,sizeof in,s->rows,s->columns,
                        s->amiga_pens?XTY_COLOR_PENS:0))
                    ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
                else {
                    if(s->presentation_features) {
                        memcpy(&in,(void *)p->dp_Arg2,sizeof in);
                        in.values[8]|=s->presentation_features();
                        memcpy((void *)p->dp_Arg2,&in,sizeof in);
                    }
                    ReplyPkt(p,DOSTRUE,0);
                }
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
            else {
                /* The read being served, or one still in line. */
                struct DosPacket *read=NULL;
                unsigned i;
                if(s->pending_read && (SIPTR)s->pending_read==p->dp_Arg2 &&
                   s->pending_read->dp_Arg1==p->dp_Arg1 && s->pending_read->dp_Port==p->dp_Port) {
                    read=s->pending_read; read_next(s);
                } else for(i=0;i<s->read_waiting;++i) {
                    struct DosPacket *q=s->read_wait[i];
                    if((SIPTR)q==p->dp_Arg2 && q->dp_Arg1==p->dp_Arg1 && q->dp_Port==p->dp_Port) {
                        read=q;
                        for(;i+1<s->read_waiting;++i) s->read_wait[i]=s->read_wait[i+1];
                        --s->read_waiting; break;
                    }
                }
                if(read) { ReplyPkt(read,-1,ERROR_BREAK); ReplyPkt(p,DOSTRUE,0); }
                else ReplyPkt(p,DOSFALSE,ERROR_OBJECT_NOT_FOUND);
            }
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
            /* As CON:, the program that opens the console gets its breaks
               until a program reads it (writes never move the target). */
            if(s->console) aros_break_bind(&s->breaker,p->dp_Port->mp_SigTask);
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
            /* The window, as CON: gives it (a plain pointer in a BPTR):
               mainline's CliInit puts requesters on it. -1 without one. */
            info->id_VolumeNode=s->window?(BPTR)(IPTR)s->window:(BPTR)(SIPTR)-1;
            /* Fields written in place, never cleared first: another client
               may be reading the last answer (the session struct starts
               zeroed). Without a grid yet, 80x24 rather than a null unit. */
            s->info_unit.cu_XMax=(WORD)((s->columns?s->columns:80)-1);
            s->info_unit.cu_YMax=(WORD)((s->rows?s->rows:24)-1);
            s->info_unit.cu_Window=s->window;
            s->info_request.io_Unit=(struct Unit *)&s->info_unit;
            info->id_InUse=(IPTR)&s->info_request;
            ReplyPkt(p,DOSTRUE,0); break;
        }
        case XTERM_ACTION_QUERY: {
            XTermStatus *status=(XTermStatus *)p->dp_Arg2;
            if(id!=1 || !(s->handles&1))
                ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
            else if(!status || p->dp_Arg3!=sizeof *status ||
                    status->version!=XTERM_STATUS_VERSION || status->reserved)
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else if(s->pending_read || s->wait_count)
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);
            else {
                status->available=(uint32_t)s->input.length;
                status->eof=s->eof || s->eof_once?1:0;
                status->columns=s->columns; status->rows=s->rows;
                ReplyPkt(p,DOSTRUE,0);
            }
            break;
        }
        case ACTION_WAIT_CHAR:
            /* DOS sends the timeout in Arg1, not a filehandle identifier.
             * Any number of clients may wait, each with its own timeout,
             * also while reads are out (CON: keeps one and loses others). */
            if(p->dp_Arg1<0) ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            /* At the end of input the answer is true, so the next Read()
               returns 0: a program that reads only after a true answer
               would otherwise poll a closed window for good. */
            else if(s->input.length || s->eof || s->eof_once || s->stopping ||
                    (s->console && !s->line.raw && s->line.length)) ReplyPkt(p,DOSTRUE,0);
            else if(!p->dp_Arg1) ReplyPkt(p,DOSFALSE,0);
            else if(wait_start(s,p,(LONG)p->dp_Arg1))
                ReplyPkt(p,DOSFALSE,s->wait_count==XSESSION_WAITERS?ERROR_OBJECT_IN_USE:ERROR_NO_FREE_STORE);
            break;
        case ACTION_READ:
            /* A console reads from any of its handles, as CON: does: a
               program that opens CON:... with MODE_NEWFILE reads its keys
               through that same handle. */
            if((id!=1 && !(s->console && (id==2 || id==3))) ||
               !(s->handles&(1UL<<(id-1))) || p->dp_Arg3<0 ||
               (p->dp_Arg3 && !p->dp_Arg2)) ReplyPkt(p,-1,ERROR_BAD_NUMBER);
            else if(!p->dp_Arg3) ReplyPkt(p,0,0);
            else if(s->pending_read && s->read_waiting==XSESSION_READERS)
                ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);
            else {
                /* As CON:, the last program to ask for input gets the breaks,
                   also when its read waits behind another one. */
                if(s->console) aros_break_bind(&s->breaker,p->dp_Port->mp_SigTask);
                if(s->pending_read) s->read_wait[s->read_waiting++]=p;
                else s->pending_read=p;
                service(s);
            }
            break;
        case ACTION_WRITE:
            if((id!=2 && id!=3 && !(s->console && id==1)) || !(s->handles&(1UL<<(id-1))) ||
               p->dp_Arg3<0 || (p->dp_Arg3 && !p->dp_Arg2))
                ReplyPkt(p,-1,ERROR_BAD_NUMBER);
            else if(s->stopping) ReplyPkt(p,-1,XSESSION_CLOSED_ERROR);
            else if(!p->dp_Arg3) ReplyPkt(p,0,0);
            else if(s->pending_write) {
                if(s->write_waiting<XSESSION_WRITERS) s->write_wait[s->write_waiting++]=p;
                else ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);
            }
            else {
                /* Writes do not move the break target: a background job
                 * that only prints must not take Ctrl-C from the Shell. The
                 * foreground is whoever reads the console; commands run in
                 * the Shell's own process, so they are covered. */
                PERF_DO(perf_write_started=PERF_NOW(); ++perf_diag.write_packets;
                        perf_diag.write_bytes+=(unsigned long)p->dp_Arg3);
                s->pending_write=p; s->write_offset=0; service(s);
                PERF_DO(if(s->pending_write==p) ++perf_diag.write_waited);
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
    /* A console takes keys while any of its handles is open (reads come on
       any of them); a plain endpoint only with its input open. */
    if(!aros_session_active(s) || s->eof ||
       (s->external && !(s->handles&(s->console?7UL:1UL)))) return 0;
    if(s->console && !s->line.raw && n==1 && bytes[0]==3)
        return aros_session_interrupt(s)?1:0;
    /* Sessions without the native editor (--command) take the same keys. */
    if(s->console && !s->line.raw && n==1 && bytes[0]>=4 && bytes[0]<=6)
        return aros_session_break_signal(s,SIGBREAKF_CTRL_D<<(bytes[0]-4)),1;
    if(s->console && !s->line.raw && n==1 && bytes[0]==0x1c)
        return aros_session_end_input(s)?1:0;
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
    /* Ctrl-C cancels what was typed ahead, a pending Ctrl-\ included. */
    s->eof_once=0;
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
int aros_session_end_input(ArosSession *s)
{
    if(!s->console || !aros_session_active(s) || s->eof || s->line.raw) return 0;
    if(s->line.length) {
        if(XSESSION_CAPACITY-s->input.length<s->line.length) return 0;
        session_queue_write(&s->input,s->line.line,s->line.length);
        s->line.length=0;
    }
    s->eof_once=1;
    service(s); return 1;
}
int aros_session_break_signal(ArosSession *s,ULONG signals)
{
    if(!s->console || !aros_session_active(s) || s->eof || s->line.raw) return 0;
    return aros_break_send_signal(&s->breaker,signals);
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
       s->lock_mask || s->pending_read || s->pending_write || s->wait_count) return -1;
    if(s->port && s->port->mp_MsgList.lh_Head->ln_Succ) return -1;
    aros_break_close(&s->breaker);
    {
        unsigned i;
        for(i=0;i<XSESSION_WAITERS;++i) if(s->waits[i].timer) {
            wait_stop(s,i);
            CloseDevice((struct IORequest *)s->waits[i].timer);
            DeleteIORequest((struct IORequest *)s->waits[i].timer);
        }
    }
    if(s->timer_port) DeleteMsgPort(s->timer_port);
#ifdef XTERM_PROCESS_PORT_CLIENTS
    if(s->port && !s->borrowed_port) DeleteMsgPort(s->port);
#else
    if(s->port) DeleteMsgPort(s->port);
#endif
    memset(s,0,sizeof *s);
    return 0;
}
