/* Native regression for two PTY endpoint paths added 2026-09-28:
   - the Ctrl-C target follows a reader only for a read the service accepted;
     a rejected read (unknown handle, bad arguments, contention) must not
     reach the owner hook, so it cannot move the break or its PTY flags;
   - DOS locks on PTY handles (ACTION_COPY_DIR_FH, ACTION_FH_FROM_LOCK,
     ACTION_FREE_LOCK): a lock keeps the endpoint from being disposed on its
     own, and is released either by UnLock or by OpenFromLock.
   Also (review of the same day): a slave write cancelled after part of it
   was queued replies with that count and ERROR_BREAK, not -1.
   The test task sends the packets itself and polls the endpoint, as the
   other endpoint tests do; it is also the packets' sender. */
#include "../src/aros_tty_packets.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ArosTtyPackets endpoint;
static struct MsgPort *reply;
static struct DosPacket *packet,*packet2;
static struct FileHandle *master,*slave,*from_lock;
static unsigned hook_calls;
static struct Task *hook_sender;
static TtyHandle hook_handle;
static int failed;

static void check(int ok,const char *what,int line)
{
    if(!ok) { printf("BREAK/LOCKS FAIL line=%d: %s\n",line,what);failed=1; }
}
#define CHECK(e) check((e),#e,__LINE__)

static TtyStatus hook(void *context,struct Task *sender,struct Task *target,struct Task **old,TtyHandle handle)
{
    (void)context;(void)target;
    ++hook_calls;hook_sender=sender;hook_handle=handle;*old=NULL;
    return TTY_OK;
}
static void send(struct DosPacket *p,TtyEnd end,LONG action,SIPTR a1,SIPTR a2,SIPTR a3)
{
    p->dp_Type=action;p->dp_Arg1=a1;p->dp_Arg2=a2;p->dp_Arg3=a3;
    SendPkt(p,endpoint.ports[end],reply);
}
/* Poll until this packet's reply arrives; other replies are not expected. */
static int wait_reply(struct DosPacket *p)
{
    unsigned tick;
    for(tick=0;tick<500;++tick) {
        struct Message *m;
        aros_tty_packets_poll(&endpoint);
        if((m=GetMsg(reply))) return m==p->dp_Link;
        Delay(1);
    }
    return 0;
}
static int pending(void)
{
    aros_tty_packets_poll(&endpoint);
    return !reply->mp_MsgList.lh_Head->ln_Succ;
}

int main(void)
{
    unsigned char in[4],out[4]={'x','y','z','!'};
    BPTR lock,lock2,lock3;
    TtyHandle derived;
    unsigned before;
    reply=CreateMsgPort();packet=AllocDosObject(DOS_STDPKT,NULL);packet2=AllocDosObject(DOS_STDPKT,NULL);
    master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL);
    from_lock=AllocDosObject(DOS_FILEHANDLE,NULL);
    if(!reply || !packet || !packet2 || !master || !slave || !from_lock ||
       aros_tty_packets_init(&endpoint,master,slave)) { puts("BREAK/LOCKS SETUP FAIL");return 20; }
    endpoint.change_signal=hook;

    /* Rejected reads: unknown handle, then a negative length. */
    send(packet,TTY_SLAVE,ACTION_READ,99,(SIPTR)in,1);
    CHECK(wait_reply(packet) && packet->dp_Res1==-1 && packet->dp_Res2==ERROR_INVALID_LOCK);
    send(packet,TTY_SLAVE,ACTION_READ,slave->fh_Arg1,(SIPTR)in,-1);
    CHECK(wait_reply(packet) && packet->dp_Res1==-1 && packet->dp_Res2==ERROR_BAD_NUMBER);
    /* The master side is the frontend itself and never moves the break. */
    send(packet,TTY_MASTER,ACTION_READ,master->fh_Arg1,(SIPTR)in,1);
    CHECK(pending());
    CHECK(hook_calls==0);

    /* An accepted slave read reaches the hook once, from this task. */
    send(packet2,TTY_SLAVE,ACTION_READ,slave->fh_Arg1,(SIPTR)in,1);
    CHECK(pending());
    CHECK(hook_calls==1 && hook_sender==FindTask(NULL) && hook_handle==0);
    /* Complete both pending reads: a master write feeds the slave read,
       a slave write feeds the master read. Four replies in all. */
    {
        struct DosPacket *w1=AllocDosObject(DOS_STDPKT,NULL),*w2=AllocDosObject(DOS_STDPKT,NULL);
        unsigned got=0,tick;
        CHECK(w1 && w2);
        if(w1 && w2) {
            send(w1,TTY_MASTER,ACTION_WRITE,master->fh_Arg1,(SIPTR)out,1);
            send(w2,TTY_SLAVE,ACTION_WRITE,slave->fh_Arg1,(SIPTR)out,1);
            for(tick=0;tick<500 && got<4;++tick) {
                struct Message *m;aros_tty_packets_poll(&endpoint);
                while((m=GetMsg(reply))) ++got;
                Delay(1);
            }
        }
        CHECK(got==4);
        /* The slave write is not a read and must not have moved the break. */
        CHECK(hook_calls==1);
        if(w1) FreeDosObject(DOS_STDPKT,w1);
        if(w2) FreeDosObject(DOS_STDPKT,w2);
    }
    before=hook_calls;

    /* Locks. A lock on the slave handle... */
    send(packet,TTY_SLAVE,ACTION_COPY_DIR_FH,slave->fh_Arg1,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1!=0);
    lock=(BPTR)packet->dp_Res1;
    /* ...on an unknown handle is refused. */
    send(packet,TTY_SLAVE,ACTION_COPY_DIR_FH,99,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1==0 && packet->dp_Res2==ERROR_INVALID_LOCK);
    /* A foreign lock pointer is refused by OpenFromLock and UnLock. */
    {
        struct FileLock fake;memset(&fake,0,sizeof fake);fake.fl_Task=endpoint.ports[TTY_SLAVE];fake.fl_Key=12345;
        send(packet,TTY_SLAVE,ACTION_FH_FROM_LOCK,(SIPTR)MKBADDR(from_lock),(SIPTR)MKBADDR(&fake),0);
        CHECK(wait_reply(packet) && packet->dp_Res1==DOSFALSE && packet->dp_Res2==ERROR_OBJECT_NOT_FOUND);
        send(packet,TTY_SLAVE,ACTION_FREE_LOCK,(SIPTR)MKBADDR(&fake),0,0);
        CHECK(wait_reply(packet) && packet->dp_Res1==DOSFALSE && packet->dp_Res2==ERROR_INVALID_LOCK);
    }
    /* OpenFromLock consumes the lock; the new handle carries data. */
    send(packet,TTY_SLAVE,ACTION_FH_FROM_LOCK,(SIPTR)MKBADDR(from_lock),(SIPTR)lock,0);
    CHECK(wait_reply(packet) && packet->dp_Res1==DOSTRUE);
    derived=(TtyHandle)from_lock->fh_Arg1;
    CHECK(derived!=0 && derived!=(TtyHandle)slave->fh_Arg1 && from_lock->fh_Type==endpoint.ports[TTY_SLAVE]);
    send(packet,TTY_SLAVE,ACTION_WRITE,(SIPTR)derived,(SIPTR)out,4);
    CHECK(wait_reply(packet) && packet->dp_Res1==4);
    send(packet,TTY_MASTER,ACTION_READ,master->fh_Arg1,(SIPTR)in,4);
    CHECK(wait_reply(packet) && packet->dp_Res1==4 && !memcmp(in,out,4));
    /* UnLock releases a lock that was never opened. */
    send(packet,TTY_SLAVE,ACTION_COPY_DIR_FH,slave->fh_Arg1,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1!=0);
    lock2=(BPTR)packet->dp_Res1;
    send(packet,TTY_SLAVE,ACTION_FREE_LOCK,(SIPTR)lock2,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1==DOSTRUE);
    /* Cancel a write halfway: 1024 of 2048 bytes fit the queue. */
    {
        static unsigned char big[2048],drained[1024];
        struct DosPacket *cancel=AllocDosObject(DOS_STDPKT,NULL);
        unsigned got=0,tick;int write_ok=0,cancel_ok=0;
        CHECK(cancel!=NULL);
        if(cancel) {
            memset(big,'P',sizeof big);
            send(packet,TTY_SLAVE,ACTION_WRITE,slave->fh_Arg1,(SIPTR)big,sizeof big);
            CHECK(pending());
            cancel->dp_Arg4=XTY_PACKET_MAGIC;cancel->dp_Arg5=0;
            send(cancel,TTY_SLAVE,XTY_ACTION_CANCEL,slave->fh_Arg1,(SIPTR)packet,0);
            for(tick=0;tick<500 && got<2;++tick) {
                struct Message *m;aros_tty_packets_poll(&endpoint);
                while((m=GetMsg(reply))) {
                    ++got;
                    if(m==packet->dp_Link) write_ok=packet->dp_Res1==1024 && packet->dp_Res2==ERROR_BREAK;
                    if(m==cancel->dp_Link) cancel_ok=cancel->dp_Res1==DOSTRUE;
                }
                Delay(1);
            }
            CHECK(got==2 && write_ok && cancel_ok);
            send(packet,TTY_MASTER,ACTION_READ,master->fh_Arg1,(SIPTR)drained,sizeof drained);
            CHECK(wait_reply(packet) && packet->dp_Res1==1024 && drained[0]=='P' && drained[1023]=='P');
            FreeDosObject(DOS_STDPKT,cancel);
        }
    }
    /* Writes and lock traffic did not move the break. */
    CHECK(hook_calls==before);

    /* A lock alone keeps the endpoint from being disposed. */
    send(packet,TTY_SLAVE,ACTION_COPY_DIR_FH,slave->fh_Arg1,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1!=0);
    lock3=(BPTR)packet->dp_Res1;
    aros_tty_packets_stop(&endpoint);
    send(packet,TTY_SLAVE,ACTION_END,(SIPTR)derived,0,0);CHECK(wait_reply(packet));
    send(packet,TTY_SLAVE,ACTION_END,slave->fh_Arg1,0,0);CHECK(wait_reply(packet));
    send(packet,TTY_MASTER,ACTION_END,master->fh_Arg1,0,0);CHECK(wait_reply(packet));
    CHECK(aros_tty_packets_dispose(&endpoint)==-1);
    send(packet,TTY_SLAVE,ACTION_FREE_LOCK,(SIPTR)lock3,0,0);
    CHECK(wait_reply(packet) && packet->dp_Res1==DOSTRUE);
    CHECK(aros_tty_packets_dispose(&endpoint)==0);

    FreeDosObject(DOS_FILEHANDLE,master);FreeDosObject(DOS_FILEHANDLE,slave);FreeDosObject(DOS_FILEHANDLE,from_lock);
    FreeDosObject(DOS_STDPKT,packet);FreeDosObject(DOS_STDPKT,packet2);DeleteMsgPort(reply);
    puts(failed?"BREAK/LOCKS FAIL":"BREAK/LOCKS PASS: rejected reads, accepted read, lock life cycle, partial cancel");
    return failed?20:0;
}
