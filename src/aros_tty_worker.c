#include "aros_tty_worker.h"
#include "aros_tty_packets.h"
#include "aros_tty_signal.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <exec/memory.h>

enum { WORKER_STOP=1, WORKER_FINISH=2, WORKER_DUPLICATE, WORKER_CONFIGURE,
       WORKER_GET_CONFIG, WORKER_SET_GEOMETRY, WORKER_GET_GEOMETRY, WORKER_SIGNAL_TARGET };
typedef struct {
    struct Message message;
    int command,result;
    TtyHandle source;
    struct FileHandle *target;
    struct Task *signal_target;
    TtyConfig config;
    TtyGeometry geometry;
} WorkerCommand;
struct ArosTtyWorker {
    ArosTtyPackets packets;
    ArosTtySignal signal;
    struct MsgPort *reply,*control;
    struct Task *owner;
    struct FileHandle *master,*slave;
    ULONG child_id;
    WorkerCommand command;
};

static void worker_entry(void)
{
    ArosTtyWorker *w=(ArosTtyWorker *)FindTask(NULL)->tc_UserData;
    Wait(SIGBREAKF_CTRL_F); /* Creator records the extended task ID first. */
    w->control=CreateMsgPort();
    w->command.result=-1;
    if(w->control && !aros_tty_packets_init(&w->packets,w->master,w->slave))
        w->command.result=0;
    if(w->command.result) {
        if(w->control) DeleteMsgPort(w->control);
        w->control=NULL;
        PutMsg(w->reply,&w->command.message);
        return;
    }
    PutMsg(w->reply,&w->command.message);
    for(;;) {
        struct Message *message;
        Wait((1UL<<w->control->mp_SigBit)|
             (1UL<<w->packets.ports[TTY_MASTER]->mp_SigBit)|
             (1UL<<w->packets.ports[TTY_SLAVE]->mp_SigBit)|
             (1UL<<w->packets.timer_port->mp_SigBit));
        while((message=GetMsg(w->control))) {
            WorkerCommand *c=(WorkerCommand *)message;
            c->result=-1;
            if(c->command==WORKER_STOP) {
                aros_tty_signal_close(&w->signal);
                aros_tty_packets_stop(&w->packets);c->result=0;
            } else if(c->command==WORKER_SIGNAL_TARGET) {
                if(w->packets.stopping) c->result=TTY_CLOSED;
                else if(c->signal_target)
                    c->result=aros_tty_signal_open(&w->signal,&w->packets.service,c->signal_target);
                else { aros_tty_signal_close(&w->signal);c->result=TTY_OK; }
            } else if(c->command==WORKER_DUPLICATE) {
                c->result=aros_tty_packets_duplicate(&w->packets,c->source,c->target);
            } else if(c->command==WORKER_CONFIGURE) {
                c->result=(((c->config.local_flags&(TTY_ISIG|TTY_NOFLSH)) || c->config.intr) &&
                    !w->packets.service.interrupt)?TTY_UNSUPPORTED:
                    tty_pair_configure(&w->packets.service.pair,&c->config);
            } else if(c->command==WORKER_GET_CONFIG) {
                c->result=tty_pair_get_config(&w->packets.service.pair,c->config.version,&c->config);
            } else if(c->command==WORKER_SET_GEOMETRY) {
                c->result=tty_pair_set_geometry(&w->packets.service.pair,&c->geometry);
            } else if(c->command==WORKER_GET_GEOMETRY) {
                c->result=tty_pair_get_geometry(&w->packets.service.pair,c->geometry.version,&c->geometry);
            } else if(c->command==WORKER_FINISH && tty_service_retired(&w->packets.service)) {
                aros_tty_signal_close(&w->signal);
                if(!aros_tty_packets_dispose(&w->packets)) {
                    c->result=0;
                    DeleteMsgPort(w->control);w->control=NULL;
                    ReplyMsg(message);
                    return;
                }
            }
            ReplyMsg(message);
        }
        aros_tty_packets_poll(&w->packets);
    }
}
static void reap(ArosTtyWorker *w)
{
    while(ChildStatus(w->child_id)!=CHILD_EXITED) Delay(1);
    ChildFree(w->child_id);w->child_id=0;
}
ArosTtyWorker *aros_tty_worker_start(BPTR *master,BPTR *slave)
{
    ArosTtyWorker *w;struct Process *child;
    if(!master || !slave || master==slave) return NULL;
    w=AllocVec(sizeof *w,MEMF_PUBLIC|MEMF_CLEAR);
    if(!w) return NULL;
    w->owner=FindTask(NULL);
    w->reply=CreateMsgPort();
    w->master=AllocDosObject(DOS_FILEHANDLE,NULL);
    w->slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    if(!w->reply || !w->master || !w->slave) goto failed;
    w->command.message.mn_ReplyPort=w->reply;
    w->command.message.mn_Length=sizeof w->command;
    child=CreateNewProcTags(NP_Entry,(IPTR)worker_entry,NP_UserData,(IPTR)w,
        NP_Name,(IPTR)"XTerm TTY service",NP_StackSize,65536,NP_NotifyOnDeath,TRUE,
        NP_Input,BNULL,NP_Output,BNULL,NP_Error,BNULL,
        NP_CloseInput,FALSE,NP_CloseOutput,FALSE,NP_CloseError,FALSE,TAG_DONE);
    if(!child) goto failed;
    w->child_id=GetETaskID(&child->pr_Task);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    WaitPort(w->reply);GetMsg(w->reply);
    if(w->command.result) { reap(w);goto failed; }
    *master=MKBADDR(w->master);*slave=MKBADDR(w->slave);
    /* Handles now belong to the caller. The child never accesses these fields
       again after the ready handshake; ordinary Close may free either one. */
    return w;
failed:
    if(w->master) FreeDosObject(DOS_FILEHANDLE,w->master);
    if(w->slave) FreeDosObject(DOS_FILEHANDLE,w->slave);
    if(w->reply) DeleteMsgPort(w->reply);
    FreeVec(w);return NULL;
}
static int command(ArosTtyWorker *w,int operation)
{
    if(!w || FindTask(NULL)!=w->owner) return -1;
    w->command.command=operation;
    PutMsg(w->control,&w->command.message);
    WaitPort(w->reply);GetMsg(w->reply);
    return w->command.result;
}
int aros_tty_worker_stop(ArosTtyWorker *w)
{ return command(w,WORKER_STOP); }
int aros_tty_worker_finish(ArosTtyWorker *w)
{
    if(command(w,WORKER_FINISH)) return -1;
    reap(w);DeleteMsgPort(w->reply);FreeVec(w);return 0;
}
static int owned(const ArosTtyWorker *w)
{ return w && FindTask(NULL)==w->owner; }
TtyStatus aros_tty_worker_signal_target(ArosTtyWorker *w,struct Task *task)
{
    TtyStatus status;if(!owned(w)) return TTY_INVALID;
    w->command.signal_target=task;status=(TtyStatus)command(w,WORKER_SIGNAL_TARGET);
    w->command.signal_target=NULL;return status;
}
TtyStatus aros_tty_worker_duplicate(ArosTtyWorker *w,BPTR source,BPTR *copy)
{
    struct FileHandle *fh,*target;TtyStatus status;
    if(!owned(w) || !source || !copy) return TTY_INVALID;
    fh=BADDR(source);
    if(fh->fh_Type!=w->packets.ports[TTY_MASTER] &&
       fh->fh_Type!=w->packets.ports[TTY_SLAVE]) return TTY_INVALID;
    target=AllocDosObject(DOS_FILEHANDLE,NULL);
    if(!target) return TTY_BUSY;
    w->command.source=(TtyHandle)fh->fh_Arg1;w->command.target=target;
    status=(TtyStatus)command(w,WORKER_DUPLICATE);
    w->command.target=NULL;
    if(status==TTY_OK) *copy=MKBADDR(target);
    else FreeDosObject(DOS_FILEHANDLE,target);
    return status;
}
TtyStatus aros_tty_worker_configure(ArosTtyWorker *w,const TtyConfig *config)
{
    if(!owned(w) || !config) return TTY_INVALID;
    w->command.config=*config;
    return (TtyStatus)command(w,WORKER_CONFIGURE);
}
TtyStatus aros_tty_worker_get_config(ArosTtyWorker *w,uint32_t version,TtyConfig *config)
{
    TtyStatus status;
    if(!owned(w) || !config) return TTY_INVALID;
    w->command.config.version=version;
    status=(TtyStatus)command(w,WORKER_GET_CONFIG);
    if(status==TTY_OK) *config=w->command.config;
    return status;
}
TtyStatus aros_tty_worker_set_geometry(ArosTtyWorker *w,const TtyGeometry *geometry)
{
    if(!owned(w) || !geometry) return TTY_INVALID;
    w->command.geometry=*geometry;
    return (TtyStatus)command(w,WORKER_SET_GEOMETRY);
}
TtyStatus aros_tty_worker_get_geometry(ArosTtyWorker *w,uint32_t version,TtyGeometry *geometry)
{
    TtyStatus status;
    if(!owned(w) || !geometry) return TTY_INVALID;
    w->command.geometry.version=version;
    status=(TtyStatus)command(w,WORKER_GET_GEOMETRY);
    if(status==TTY_OK) *geometry=w->command.geometry;
    return status;
}
