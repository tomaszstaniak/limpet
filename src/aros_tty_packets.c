#include "console_geometry.h"
#include "aros_tty_packets.h"
#include "xterm_presentation.h"
#include "tty_control_server.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

static LONG status_error(TtyStatus status)
{
    switch(status) {
    case TTY_CLOSED: return ERROR_INVALID_LOCK;
    case TTY_BUSY: case TTY_AGAIN: return ERROR_OBJECT_IN_USE;
    case TTY_UNSUPPORTED: return ERROR_ACTION_NOT_KNOWN;
    default: return ERROR_BAD_NUMBER;
    }
}
static void finish_wait(ArosTtyPackets *s,TtyEnd end,LONG value,LONG error)
{
    ArosTtyWait *w=&s->waits[end];struct DosPacket *p=w->packet;
    if(!p) return;
    if(!CheckIO((struct IORequest *)w->timer)) AbortIO((struct IORequest *)w->timer);
    WaitIO((struct IORequest *)w->timer);
    w->packet=NULL;
    ReplyPkt(p,value,error);
}
static void service_waits(ArosTtyPackets *s)
{
    unsigned end;
    for(end=0;end<2;++end) if(s->waits[end].packet) {
        TtyStatus status=tty_pair_read_status(&s->service.pair,(TtyEnd)end);
        if(s->stopping || status==TTY_CLOSED)
            finish_wait(s,(TtyEnd)end,DOSFALSE,ERROR_BREAK);
        else if(status==TTY_OK || status==TTY_EOF)
            finish_wait(s,(TtyEnd)end,status==TTY_OK?DOSTRUE:DOSFALSE,0);
        else if(CheckIO((struct IORequest *)s->waits[end].timer))
            finish_wait(s,(TtyEnd)end,DOSFALSE,
                        s->waits[end].timer->tr_node.io_Error?ERROR_BREAK:0);
    }
}
static int start_wait(ArosTtyPackets *s,TtyEnd end,struct DosPacket *p)
{
    ArosTtyWait *w=&s->waits[end];
    if(!w->timer) {
        w->timer=(struct timerequest *)CreateIORequest(s->timer_port,sizeof *w->timer);
        if(!w->timer) return -1;
        if(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)w->timer,0)) {
            DeleteIORequest((struct IORequest *)w->timer);w->timer=NULL;return -1;
        }
    }
    w->timer->tr_node.io_Command=TR_ADDREQUEST;
    w->timer->tr_time.tv_secs=p->dp_Arg1/1000000;
    w->timer->tr_time.tv_micro=p->dp_Arg1%1000000;
    w->packet=p;SendIO((struct IORequest *)w->timer);return 0;
}
static void stop_read_timer(ArosTtyPackets *s)
{
    if(!s->timed_read) return;
    if(!CheckIO((struct IORequest *)s->read_timer)) AbortIO((struct IORequest *)s->read_timer);
    WaitIO((struct IORequest *)s->read_timer);
    s->timed_read=NULL;s->read_timer_token=0;
}
static void sync_read_timer(ArosTtyPackets *s)
{
    TtyRequest *r;uint64_t token;unsigned ds=tty_service_read_timer(&s->service,&r,&token);
    if(s->timed_read==r && s->read_timer_token==token) return;
    stop_read_timer(s);
    if(!ds || s->stopping) return;
    s->read_timer->tr_node.io_Command=TR_ADDREQUEST;
    s->read_timer->tr_time.tv_secs=ds/10;
    s->read_timer->tr_time.tv_micro=(ds%10)*100000;
    s->timed_read=r;s->read_timer_token=token;
    SendIO((struct IORequest *)s->read_timer);
}
static void flush_replies(ArosTtyPackets *s)
{
    TtyRequest *r;
    while((r=tty_service_take(&s->service))) {
        size_t i;
        if(s->timed_read==r) stop_read_timer(s);
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(r==&s->slots[i].request) {
            struct DosPacket *p=s->slots[i].packet;
            int control=p->dp_Type==XTY_ACTION_CONTROL;
            SIPTR value=control?DOSFALSE:-1;LONG error;
            /* A cancelled write that moved bytes returns their count, with
               the reason in Res2, so the caller does not send them twice. */
            if(r->cancelled && r->result.status==TTY_OK && r->result.count && !control) {
                value=(SIPTR)r->result.count;error=ERROR_BREAK;
            } else if(r->cancelled) error=ERROR_BREAK;
            else if(r->result.status==TTY_OK || r->result.status==TTY_EOF) {
                value=control?DOSTRUE:(SIPTR)r->result.count;error=0;
                if(control) {
                    s->slots[i].control.reserved[0]=XTY_PACKET_MAGIC;
                    memcpy((void *)p->dp_Arg2,&s->slots[i].control,sizeof(XtyControl));
                }
            } else error=status_error(r->result.status);
            /* No caller-owned packet or buffer is accessed after ReplyPkt. */
            s->slots[i].packet=NULL;
            ReplyPkt(p,value,error);
            break;
        }
    }
}
int aros_tty_packets_init(ArosTtyPackets *s,struct FileHandle *master,struct FileHandle *slave)
{
    if(!s || !master || !slave || master==slave) return -1;
    memset(s,0,sizeof *s);
    s->ports[TTY_MASTER]=CreateMsgPort();
    if(!s->ports[TTY_MASTER]) return -1;
    s->ports[TTY_SLAVE]=CreateMsgPort();
    if(!s->ports[TTY_SLAVE]) {
        DeleteMsgPort(s->ports[TTY_MASTER]);s->ports[TTY_MASTER]=NULL;
        return -1;
    }
    s->timer_port=CreateMsgPort();
    if(!s->timer_port) {
        DeleteMsgPort(s->ports[TTY_MASTER]);DeleteMsgPort(s->ports[TTY_SLAVE]);
        s->ports[TTY_MASTER]=s->ports[TTY_SLAVE]=NULL;return -1;
    }
    s->read_timer=(struct timerequest *)CreateIORequest(s->timer_port,sizeof *s->read_timer);
    if(!s->read_timer || OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)s->read_timer,0)) {
        if(s->read_timer) DeleteIORequest((struct IORequest *)s->read_timer);
        s->read_timer=NULL;DeleteMsgPort(s->timer_port);s->timer_port=NULL;
        DeleteMsgPort(s->ports[TTY_MASTER]);DeleteMsgPort(s->ports[TTY_SLAVE]);
        s->ports[TTY_MASTER]=s->ports[TTY_SLAVE]=NULL;return -1;
    }
    tty_service_init(&s->service);
    s->exports[0]=1;s->exports[1]=2;
    s->export_ends[0]=TTY_MASTER;s->export_ends[1]=TTY_SLAVE;
    master->fh_Type=s->ports[TTY_MASTER];slave->fh_Type=s->ports[TTY_SLAVE];
    master->fh_Arg1=1;slave->fh_Arg1=2;
    master->fh_Interactive=slave->fh_Interactive=DOSTRUE;
    return 0;
}
TtyStatus aros_tty_packets_duplicate(ArosTtyPackets *s,TtyHandle source,struct FileHandle *fh)
{
    size_t i,j;TtyHandle id;TtyStatus status;
    if(!s || !fh) return TTY_INVALID;
    if(!s->ports[TTY_MASTER] || s->stopping) return TTY_CLOSED;
    for(j=0;j<TTY_SERVICE_HANDLES;++j)
        if(source && s->exports[j]==source) break;
    if(j==TTY_SERVICE_HANDLES) return TTY_CLOSED;
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(!s->exports[i]) break;
    if(i==TTY_SERVICE_HANDLES || s->service.next_handle>(uint64_t)INTPTR_MAX)
        return TTY_BUSY;
    status=tty_service_duplicate(&s->service,source,&id);
    if(status!=TTY_OK) return status;
    s->exports[i]=id;s->export_ends[i]=s->export_ends[j];
    fh->fh_Type=s->ports[s->export_ends[i]];fh->fh_Arg1=(SIPTR)id;
    fh->fh_Interactive=DOSTRUE;
    return TTY_OK;
}
/* DOS locks on a PTY handle, as con-handler has them: DupLockFromFH and
   OpenFromLock are how C:Run and SystemTagList(SYS_DupStream) duplicate a
   console stream. A lock owns its own duplicated service handle, so the
   stream stays open while only the lock exists; OpenFromLock hands that
   handle to the new FileHandle and frees the lock, UnLock closes it. */
static size_t lock_export(ArosTtyPackets *s,TtyEnd end,BPTR lock)
{
    struct FileLock *fl=BADDR(lock);size_t i;
    if(!fl || fl->fl_Task!=s->ports[end]) return TTY_SERVICE_HANDLES;
    for(i=0;i<TTY_SERVICE_HANDLES;++i)
        if(s->exports[i] && (IPTR)s->exports[i]==fl->fl_Key && s->export_ends[i]==end) break;
    return i;
}
static void dispatch_lock(ArosTtyPackets *s,TtyEnd end,struct DosPacket *p)
{
    size_t i,j;TtyHandle id;TtyStatus status;struct FileLock *fl;
    if(p->dp_Type==ACTION_COPY_DIR_FH) {
        for(j=0;j<TTY_SERVICE_HANDLES;++j)
            if(s->exports[j] && s->exports[j]==(TtyHandle)p->dp_Arg1 && s->export_ends[j]==end) break;
        if(j==TTY_SERVICE_HANDLES || s->stopping) { ReplyPkt(p,(SIPTR)BNULL,ERROR_INVALID_LOCK);return; }
        for(i=0;i<TTY_SERVICE_HANDLES;++i) if(!s->exports[i]) break;
        if(i==TTY_SERVICE_HANDLES) { ReplyPkt(p,(SIPTR)BNULL,ERROR_NO_FREE_STORE);return; }
        if(!(fl=AllocMem(sizeof *fl,MEMF_PUBLIC|MEMF_CLEAR))) { ReplyPkt(p,(SIPTR)BNULL,ERROR_NO_FREE_STORE);return; }
        status=tty_service_duplicate(&s->service,s->exports[j],&id);
        if(status!=TTY_OK) { FreeMem(fl,sizeof *fl);ReplyPkt(p,(SIPTR)BNULL,status_error(status));return; }
        s->exports[i]=id;s->export_ends[i]=end;
        fl->fl_Task=s->ports[end];fl->fl_Access=ACCESS_READ;fl->fl_Key=(IPTR)id;
        ReplyPkt(p,(SIPTR)MKBADDR(fl),0);return;
    }
    if(p->dp_Type==ACTION_FH_FROM_LOCK) {
        struct FileHandle *fh=BADDR(p->dp_Arg1);
        i=lock_export(s,end,(BPTR)p->dp_Arg2);
        if(!fh || i==TTY_SERVICE_HANDLES || s->stopping) { ReplyPkt(p,DOSFALSE,ERROR_OBJECT_NOT_FOUND);return; }
        fl=BADDR((BPTR)p->dp_Arg2);
        fh->fh_Type=s->ports[end];fh->fh_Arg1=(SIPTR)s->exports[i];fh->fh_Interactive=DOSTRUE;
        FreeMem(fl,sizeof *fl); /* The lock is consumed only on success. */
        ReplyPkt(p,DOSTRUE,0);return;
    }
    /* ACTION_FREE_LOCK */
    i=lock_export(s,end,(BPTR)p->dp_Arg1);
    if(i==TTY_SERVICE_HANDLES) { ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);return; }
    status=tty_service_close(&s->service,s->exports[i]);
    if(s->stopping && status==TTY_CLOSED) status=TTY_OK;
    if(status==TTY_OK) {
        s->exports[i]=0;FreeMem(BADDR((BPTR)p->dp_Arg1),sizeof(struct FileLock));
    }
    flush_replies(s);service_waits(s);
    ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,status==TTY_OK?0:status_error(status));
}
/* The task that reads the terminal is its foreground and gets Ctrl-C.
   con-handler moves its breaktask on every READ and WRITE; following writes
   too would let a background job that prints take the break, so only reads
   count here. Called only for a read the service has accepted and before
   its reply is sent: a rejected packet (wrong handle, bad arguments) must
   not move the break, and until the reply the sender is waiting and alive
   to bind. Slave side only: the master side is the frontend itself. */
static void follow_reader(ArosTtyPackets *s,TtyEnd end,struct DosPacket *p)
{
    struct Task *sender,*old;
    if(end!=TTY_SLAVE || !s->change_signal || s->stopping) return;
    sender=p->dp_Port->mp_SigTask;
    (void)s->change_signal(s->change_signal_context,sender,sender,&old,0);
}
static void dispatch(ArosTtyPackets *s,TtyEnd end,struct DosPacket *p)
{
    TtyStatus status;size_t i;TtyRequest *r;
    if(p->dp_Type==ACTION_READ || p->dp_Type==ACTION_WRITE || p->dp_Type==ACTION_END ||
       p->dp_Type==XTY_ACTION_TRY_READ || p->dp_Type==XTY_ACTION_TRY_WRITE ||
       p->dp_Type==XTY_ACTION_CONTROL || p->dp_Type==XTY_ACTION_CANCEL ||
       p->dp_Type==ACTION_CONSOLE_GETGEOMETRY) {
        for(i=0;i<TTY_SERVICE_HANDLES;++i)
            if(s->exports[i] && s->exports[i]==(TtyHandle)p->dp_Arg1 &&
               s->export_ends[i]==end) break;
        if(i==TTY_SERVICE_HANDLES) {
            ReplyPkt(p,(p->dp_Type==ACTION_END || p->dp_Type==XTY_ACTION_CONTROL || p->dp_Type==XTY_ACTION_CANCEL ||
                        p->dp_Type==ACTION_CONSOLE_GETGEOMETRY)?DOSFALSE:-1,
                     ERROR_INVALID_LOCK);return;
        }
    }
    switch(p->dp_Type) {
    case ACTION_CONSOLE_GETGEOMETRY: {
        const TtyGeometry *g=&s->service.pair.geometry;
        if(s->stopping) ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
        else if(!s->presentation_enabled) ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
        else if(!console_geometry_reply((void *)p->dp_Arg2,(SIPTR)p->dp_Arg3,
                    p->dp_Arg4,g->columns,g->rows,g->xpixel,g->ypixel))
            ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
        else ReplyPkt(p,DOSTRUE,0);
        return;
    }
    case XTY_ACTION_CANCEL:
        if(!p->dp_Arg2 || p->dp_Arg3 || p->dp_Arg4!=XTY_PACKET_MAGIC || p->dp_Arg5) {
            ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);return;
        }
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            struct DosPacket *target=s->slots[i].packet;
            /* Compare identity first; never dereference the supplied address.
               A returned packet may already belong to a different operation. */
            if(target && (SIPTR)target==p->dp_Arg2 && target->dp_Arg1==p->dp_Arg1 &&
               target->dp_Port==p->dp_Port) {
                status=aros_tty_packets_cancel(s,target);
                ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,
                    status==TTY_OK?0:status_error(status));return;
            }
        }
        ReplyPkt(p,DOSFALSE,ERROR_OBJECT_NOT_FOUND);return;
    case XTY_ACTION_CONTROL: {
        XtyControl control;
        /* Pending operations are cancelled with BREAK. Fresh negotiation on
           a retired endpoint is terminal, not an endlessly retryable EINTR. */
        if(s->stopping) { ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);return; }
        if(p->dp_Arg4!=XTY_PACKET_MAGIC || p->dp_Arg5 ||
           p->dp_Arg3!=XTY_CONTROL_BYTES || !p->dp_Arg2) {
            ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);return;
        }
        memcpy(&control,(void *)p->dp_Arg2,sizeof control);
        if(control.operation==XTY_GET_IDENTITY) {
            if(!s->presentation_enabled || end!=TTY_SLAVE)
                ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
            else if(xterm_identity_reply((void *)p->dp_Arg2,sizeof control,XTERM_IDENTITY_NAME,LIMPET_VERSION))
                ReplyPkt(p,DOSTRUE,0);
            else ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            return;
        }
        if(control.operation==XTY_GET_PRESENTATION) {
            if(!s->presentation_enabled || end!=TTY_SLAVE)
                ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);
            else if(!xterm_presentation_reply((void *)p->dp_Arg2,sizeof control,
                    s->service.pair.geometry.rows,s->service.pair.geometry.columns,0))
                ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);
            else {
                    if(s->presentation_features) {
                        memcpy(&control,(void *)p->dp_Arg2,sizeof control);
                        control.values[8]|=s->presentation_features();
                        memcpy((void *)p->dp_Arg2,&control,sizeof control);
                    }
                    ReplyPkt(p,DOSTRUE,0);
                }
            return;
        }
        if((control.operation==XTY_SET_CONFIG || control.operation==XTY_SET_CONFIG_DISCARD_INPUT ||
            control.operation==XTY_DRAIN_CONFIG || control.operation==XTY_DRAIN_CONFIG_DISCARD_INPUT) &&
           ((control.values[2]&(XTY_ISIG|XTY_NOFLSH)) || control.values[9]) && !s->service.interrupt) {
            ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);return;
        }
        if(control.operation==XTY_DRAIN || control.operation==XTY_DRAIN_CONFIG ||
           control.operation==XTY_DRAIN_CONFIG_DISCARD_INPUT) {
            TtyOperation operation;TtyConfig config;
            status=tty_control_prepare_async(end,&control,sizeof control,&operation,&config);
            if(status!=TTY_OK) { ReplyPkt(p,DOSFALSE,status_error(status));return; }
            for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(!s->slots[i].packet) break;
            if(i==TTY_SERVICE_REQUESTS) { ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);return; }
            r=&s->slots[i].request;memset(r,0,sizeof *r);
            s->slots[i].control=control;s->slots[i].config=config;
            r->handle=(TtyHandle)p->dp_Arg1;r->operation=operation;
            if(operation!=TTY_REQUEST_DRAIN) {
                r->buffer=&s->slots[i].config;r->length=sizeof config;
            }
            s->slots[i].packet=p;status=tty_service_submit(&s->service,r);
            if(status!=TTY_OK) {
                s->slots[i].packet=NULL;ReplyPkt(p,DOSFALSE,status_error(status));
            }
            flush_replies(s);return;
        }
        status=tty_control_apply(&s->service.pair,end,(void *)p->dp_Arg2,(size_t)p->dp_Arg3);
        if(status==TTY_OK) {
            /* Only this asynchronous adapter implements these capabilities;
               the immediate pair-only control dispatcher does not. */
            if(control.operation==XTY_CAPS) {
                memcpy(&control,(void *)p->dp_Arg2,sizeof control);
                control.values[0]|=XTY_CAP_CANCEL|XTY_CAP_TRY_IO;
                if(s->service.interrupt) {
                    control.values[0]|=XTY_CAP_VINTR;
                    control.values[3]|=XTY_ISIG|XTY_NOFLSH;
                }
                if(end==TTY_SLAVE)
                    control.values[0]|=XTY_CAP_DRAIN|XTY_CAP_DRAIN_CONFIG|XTY_CAP_DRAIN_CONFIG_DISCARD_INPUT|XTY_CAP_READ_TIMING;
                memcpy((void *)p->dp_Arg2,&control,sizeof control);
            }
            tty_service_pump(&s->service);flush_replies(s);service_waits(s);
        }
        ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,status==TTY_OK?0:status_error(status));
        return;
    }
    case ACTION_WAIT_CHAR:
        if(s->stopping) { ReplyPkt(p,DOSFALSE,ERROR_BREAK);return; }
        if(p->dp_Arg1<0 || p->dp_Arg1>INT32_MAX) {
            ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);return;
        }
        if(s->waits[end].packet) { ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);return; }
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
            TtyServiceSlot *slot=&s->service.slots[i];
            if(slot->request && !slot->complete && slot->end==end &&
               slot->request->operation==TTY_REQUEST_READ) {
                ReplyPkt(p,DOSFALSE,ERROR_OBJECT_IN_USE);return;
            }
        }
        status=tty_pair_read_status(&s->service.pair,end);
        if(status==TTY_CLOSED) ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);
        else if(status==TTY_OK) ReplyPkt(p,DOSTRUE,0);
        else if(status==TTY_EOF || !p->dp_Arg1) ReplyPkt(p,DOSFALSE,0);
        else if(start_wait(s,end,p)) ReplyPkt(p,DOSFALSE,ERROR_NO_FREE_STORE);
        return;
    case ACTION_SCREEN_MODE: {
        TtyConfig config;
        if(s->stopping) { ReplyPkt(p,DOSFALSE,ERROR_BREAK);return; }
        if(end!=TTY_SLAVE) { ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);return; }
        if(p->dp_Arg1!=0 && p->dp_Arg1!=1) {
            ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);return;
        }
        config=s->service.pair.config;
        /* DOS mode changes canonical/echo only; explicit mapping/output flags
           and VEOF remain under the creator's configuration control. */
        config.local_flags=p->dp_Arg1?0:TTY_CANON|TTY_ECHO;
        status=tty_pair_configure(&s->service.pair,&config);
        if(status==TTY_OK) {
            /* Disabling echo can unblock a write despite a full output ring.
               No subsequent packet is guaranteed to wake the owner again. */
            tty_service_pump(&s->service);flush_replies(s);
        }
        ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,status==TTY_OK?0:status_error(status));
        return;
    }
    case XTY_ACTION_TRY_READ: case XTY_ACTION_TRY_WRITE: {
        TtyResult result;
        if(s->stopping) { ReplyPkt(p,-1,ERROR_BREAK);return; }
        if(p->dp_Arg4!=XTY_PACKET_MAGIC || p->dp_Arg5 ||
           p->dp_Arg3<0 || p->dp_Arg3>INT32_MAX || (p->dp_Arg3 && !p->dp_Arg2)) {
            ReplyPkt(p,-1,ERROR_BAD_NUMBER);return;
        }
        /* Match the existing read/waiter exclusion policy; a readiness waiter
           is not permission for a concurrent reader to consume its input. */
        if(p->dp_Type==XTY_ACTION_TRY_READ && p->dp_Arg3 && s->waits[end].packet) {
            ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);return;
        }
        result=tty_service_try_io(&s->service,(TtyHandle)p->dp_Arg1,
            p->dp_Type==XTY_ACTION_TRY_READ?TTY_REQUEST_READ:TTY_REQUEST_WRITE,
            (void *)p->dp_Arg2,(size_t)p->dp_Arg3);
        /* No data yet is still an accepted read by a polling reader. */
        if(p->dp_Type==XTY_ACTION_TRY_READ &&
           (result.status==TTY_OK || result.status==TTY_EOF || result.status==TTY_AGAIN))
            follow_reader(s,end,p);
        flush_replies(s);
        if(result.status==TTY_OK || result.status==TTY_EOF)
            ReplyPkt(p,(SIPTR)result.count,0);
        else ReplyPkt(p,-1,status_error(result.status));
        return;
    }
    case ACTION_READ: case ACTION_WRITE:
        if(s->stopping) { ReplyPkt(p,-1,ERROR_BREAK);return; }
        if(p->dp_Arg3<0 || p->dp_Arg3>INT32_MAX || (p->dp_Arg3 && !p->dp_Arg2)) {
            ReplyPkt(p,-1,ERROR_BAD_NUMBER);return;
        }
        /* Only a read that can consume input competes with the waiter.
           Validate malformed I/O before reporting transient contention. */
        if(p->dp_Type==ACTION_READ && p->dp_Arg3 && s->waits[end].packet) {
            ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);return;
        }
        for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(!s->slots[i].packet) break;
        if(i==TTY_SERVICE_REQUESTS) { ReplyPkt(p,-1,ERROR_OBJECT_IN_USE);return; }
        r=&s->slots[i].request;memset(r,0,sizeof *r);
        r->handle=(TtyHandle)p->dp_Arg1;
        r->operation=p->dp_Type==ACTION_READ?TTY_REQUEST_READ:TTY_REQUEST_WRITE;
        r->buffer=(void *)p->dp_Arg2;r->length=(size_t)p->dp_Arg3;
        s->slots[i].packet=p;
        status=tty_service_submit(&s->service,r);
        if(status!=TTY_OK) {
            s->slots[i].packet=NULL;ReplyPkt(p,-1,status_error(status));
        } else if(p->dp_Type==ACTION_READ) follow_reader(s,end,p);
        flush_replies(s);
        return;
    case ACTION_END:
        for(i=0;i<TTY_SERVICE_HANDLES;++i)
            if(s->exports[i] && s->exports[i]==(TtyHandle)p->dp_Arg1) break;
        if(i==TTY_SERVICE_HANDLES) {
            ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);return;
        }
        status=tty_service_close(&s->service,(TtyHandle)p->dp_Arg1);
        if(s->stopping && status==TTY_CLOSED) status=TTY_OK;
        if(status==TTY_OK) s->exports[i]=0;
        /* Return cancelled reads/writes before acknowledging the handle close. */
        flush_replies(s);
        service_waits(s);
        ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,status==TTY_OK?0:status_error(status));
        return;
    case ACTION_COPY_DIR_FH: case ACTION_FH_FROM_LOCK: case ACTION_FREE_LOCK:
        dispatch_lock(s,end,p);return;
    case ACTION_CHANGE_SIGNAL: {
        /* dos.library sends Arg2 in two forms: fs_ChangeSignal (CliInit)
           passes the struct Process *, SystemTagList and CreateNewProc pass
           &process->pr_MsgPort. con-handler signals Arg2 as a Task either
           way; here the node type tells them apart. */
        struct Node *arg=(struct Node *)p->dp_Arg2;
        struct Task *target=NULL,*old=NULL;
        if(!s->change_signal) { ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);return; }
        for(i=0;i<TTY_SERVICE_HANDLES;++i)
            if(s->exports[i] && s->exports[i]==(TtyHandle)p->dp_Arg1) break;
        if(i==TTY_SERVICE_HANDLES || s->stopping) { ReplyPkt(p,DOSFALSE,ERROR_INVALID_LOCK);return; }
        if(arg) {
            if(arg->ln_Type==NT_MSGPORT) target=((struct MsgPort *)arg)->mp_SigTask;
            else if(arg->ln_Type==NT_TASK || arg->ln_Type==NT_PROCESS) target=(struct Task *)arg;
            else { ReplyPkt(p,DOSFALSE,ERROR_BAD_NUMBER);return; }
        }
        status=s->change_signal(s->change_signal_context,p->dp_Port->mp_SigTask,target,&old,
                                (TtyHandle)p->dp_Arg1);
        ReplyPkt(p,status==TTY_OK?DOSTRUE:DOSFALSE,status==TTY_OK?(SIPTR)old:status_error(status));
        return;
    }
    case ACTION_IS_FILESYSTEM:
        ReplyPkt(p,DOSFALSE,0);return;
    case ACTION_SEEK:
        ReplyPkt(p,-1,ERROR_ACTION_NOT_KNOWN);return;
    default:
        ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);return;
    }
}
void aros_tty_packets_poll(ArosTtyPackets *s)
{
    struct Message *m;unsigned end;
    if(!s || !s->ports[TTY_MASTER]) return;
    if(s->timed_read && CheckIO((struct IORequest *)s->read_timer)) {
        TtyRequest *r=s->timed_read;uint64_t token=s->read_timer_token;
        int failed=s->read_timer->tr_node.io_Error!=0;
        stop_read_timer(s);
        if(failed) tty_service_cancel(&s->service,r);
        else tty_service_read_timeout(&s->service,r,token);
    }
    tty_service_pump(&s->service);flush_replies(s);service_waits(s);
    sync_read_timer(s);
    for(end=0;end<2;++end) {
        unsigned budget=32;
        struct MsgPort *port=s->ports[end];
        while(budget-- && (m=GetMsg(port))) {
            dispatch(s,(TtyEnd)end,(struct DosPacket *)m->mn_Node.ln_Name);
            service_waits(s);
            sync_read_timer(s);
        }
        if(port->mp_MsgList.lh_Head->ln_Succ)
            Signal(FindTask(NULL),1UL<<port->mp_SigBit);
    }
}
TtyStatus aros_tty_packets_cancel(ArosTtyPackets *s,struct DosPacket *p)
{
    size_t i;TtyStatus status;
    if(!s || !p) return TTY_INVALID;
    for(i=0;i<2;++i) if(s->waits[i].packet==p) {
        finish_wait(s,(TtyEnd)i,DOSFALSE,ERROR_BREAK);return TTY_OK;
    }
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(s->slots[i].packet==p) {
        status=tty_service_cancel(&s->service,&s->slots[i].request);
        flush_replies(s);service_waits(s);sync_read_timer(s);return status;
    }
    return TTY_INVALID;
}
void aros_tty_packets_stop(ArosTtyPackets *s)
{
    size_t i;
    if(!s) return;
    s->stopping=1;
    stop_read_timer(s);
    for(i=0;i<2;++i) finish_wait(s,(TtyEnd)i,DOSFALSE,ERROR_BREAK);
    /* Freeze all pending outcomes before closing either endpoint or pumping.
       Otherwise peer close can convert cancellation into EOF, or cancelling
       one blocked write can admit a later write during shutdown. */
    tty_service_cancel_all(&s->service);
    for(i=0;i<TTY_SERVICE_HANDLES;++i)
        if(s->service.handles[i].id)
            tty_service_close(&s->service,s->service.handles[i].id);
    flush_replies(s);
    aros_tty_packets_poll(s);
}
int aros_tty_packets_dispose(ArosTtyPackets *s)
{
    size_t i;
    if(!s) return -1;
    if(!s->ports[TTY_MASTER]) return 0;
    for(i=0;i<TTY_SERVICE_HANDLES;++i) if(s->exports[i]) return -1;
    if(!tty_service_retired(&s->service) ||
       s->ports[TTY_MASTER]->mp_MsgList.lh_Head->ln_Succ ||
       s->ports[TTY_SLAVE]->mp_MsgList.lh_Head->ln_Succ) return -1;
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) if(s->slots[i].packet) return -1;
    for(i=0;i<2;++i) if(s->waits[i].packet) return -1;
    for(i=0;i<2;++i) if(s->waits[i].timer) {
        CloseDevice((struct IORequest *)s->waits[i].timer);
        DeleteIORequest((struct IORequest *)s->waits[i].timer);s->waits[i].timer=NULL;
    }
    stop_read_timer(s);
    CloseDevice((struct IORequest *)s->read_timer);
    DeleteIORequest((struct IORequest *)s->read_timer);s->read_timer=NULL;
    DeleteMsgPort(s->timer_port);s->timer_port=NULL;
    for(i=0;i<2;++i) { DeleteMsgPort(s->ports[i]);s->ports[i]=NULL; }
    return 0;
}
