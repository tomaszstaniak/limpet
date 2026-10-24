#include "../src/aros_session.h"
#include "../src/xterm_status.h"
#ifndef XTY_ACTION_CANCEL
#include "../sdk/include/limpet/tty_control.h"
#endif
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ArosSession session;
static struct MsgPort *reply;
static struct DosPacket *packet;
static struct FileHandle *files[3];
static int queued;
static ULONG child_id;
static struct DosPacket *batch[65];
static struct { BPTR fh; int ok; } client;

static void native_client(void)
{
    char bytes[8];
    Wait(SIGBREAKF_CTRL_F);
    client.ok=Read(client.fh,bytes,3)==3 && !memcmp(bytes,"ok\n",3) &&
              Write(client.fh,"NATIVE_WRITE\n",13)==13;
    if(!Close(client.fh)) client.ok=0;
}
static void fail(const char *what,int line)
{
    unsigned i,tick;
    printf("FAIL endpoint %d: %s\n",line,what); fflush(stdout);
    /* Preserve code and ports if a failed check leaves a client or lease.
     * Never unload a test with outstanding shared-address-space references. */
    aros_session_stop(&session);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) {
        aros_session_poll(&session); Delay(1);
    }
    if(child_id) { ChildFree(child_id); child_id=0; }
    while(queued) {
        aros_session_poll(&session);
        if(GetMsg(reply)) queued=0; else Delay(1);
    }
    /* All producers are ours and have now stopped. Drain synthetic packets,
     * then release their known handle/lock references through the endpoint. */
    while(session.port && session.port->mp_MsgList.lh_Head->ln_Succ)
        aros_session_poll(&session);
    if(reply) while(GetMsg(reply)) {}
    if(session.port && packet && reply) {
        for(i=0;i<19;++i) {
            while(i<3?session.references[i]:(session.lock_mask&(1U<<(i-3)))) {
                packet->dp_Type=i<3?ACTION_END:ACTION_FREE_LOCK;
                packet->dp_Arg1=i<3?(SIPTR)i+1:(SIPTR)MKBADDR(&session.locks[i-3]);
                SendPkt(packet,session.port,reply);
                for(tick=0;tick<500;++tick) {
                    aros_session_poll(&session);
                    if(GetMsg(reply)) break;
                    Delay(1);
                }
                if(tick==500 || packet->dp_Res1!=DOSTRUE) goto parked;
            }
        }
    }
    if(aros_session_dispose(&session)) goto parked;
    for(i=0;i<3;++i) if(files[i]) FreeDosObject(DOS_FILEHANDLE,files[i]);
    for(i=0;i<65;++i) if(batch[i]) FreeDosObject(DOS_STDPKT,batch[i]);
    if(packet) FreeDosObject(DOS_STDPKT,packet);
    if(reply) DeleteMsgPort(reply);
    exit(20);
parked:
    puts("Unresolved endpoint references: retaining test process for safety.");
    for(;;) { aros_session_poll(&session); Delay(50); }
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void submit(LONG action,IPTR a,IPTR b,IPTR c)
{
    CHECK(!queued);
    packet->dp_Type=action; packet->dp_Arg1=a; packet->dp_Arg2=b; packet->dp_Arg3=c;
    queued=1; SendPkt(packet,session.port,reply);
}
static SIPTR receive(void)
{
    unsigned tick;
    for(tick=0;tick<500;++tick) {
        struct Message *m;
        aros_session_poll(&session);
        if((m=GetMsg(reply))) { CHECK(m==packet->dp_Link); queued=0; return packet->dp_Res1; }
        Delay(1);
    }
    fail("packet reply deadline",__LINE__); return 0;
}
static SIPTR call(LONG action,IPTR a,IPTR b,IPTR c)
{ submit(action,a,b,c); return receive(); }
static void open_input(unsigned i)
{
    files[i]=AllocDosObject(DOS_FILEHANDLE,NULL); CHECK(files[i]);
    SetVBuf(MKBADDR(files[i]),NULL,BUF_NONE,-1);
    CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(files[i]),0,0)==DOSTRUE);
    CHECK(files[i]->fh_Type==session.port && files[i]->fh_Interactive);
}
static void close_input(unsigned i)
{
    CHECK(call(ACTION_END,files[i]->fh_Arg1,0,0)==DOSTRUE);
    FreeDosObject(DOS_FILEHANDLE,files[i]); files[i]=NULL;
}
static void start(void)
{
    CHECK(!aros_session_start_endpoint(&session));
    CHECK(!session.task_id && aros_session_active(&session));
    CHECK(aros_session_dispose(&session)==-1);
    CHECK(!aros_session_write(&session,(const unsigned char *)"x",1));
}
int main(void)
{
    BPTR lock;
    unsigned char bytes[64];
    size_t n;
    struct Process *process;
    reply=CreateMsgPort(); packet=AllocDosObject(DOS_STDPKT,NULL);
    CHECK(reply && packet);
    if(aros_session_start_endpoint(&session)) {
        puts("FAIL endpoint: external constructor");
        FreeDosObject(DOS_STDPKT,packet); DeleteMsgPort(reply); return 20;
    }
    CHECK(aros_session_active(&session));
    CHECK(aros_session_dispose(&session)==-1);
    open_input(0); open_input(1);
    session.columns=91; session.rows=27;
    {
        XTermStatus status={XTERM_STATUS_VERSION,0,0,0,0,0};
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSTRUE);
        CHECK(status.available==0 && status.eof==0 && status.reserved==0);
        CHECK(status.columns==91 && status.rows==27);
        session.columns=103; session.rows=31;
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSTRUE);
        CHECK(status.columns==103 && status.rows==31);
        status.version=2;
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSFALSE);
        CHECK(packet->dp_Res2==ERROR_BAD_NUMBER);
        status.version=XTERM_STATUS_VERSION;
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status-1)==DOSFALSE);
        CHECK(packet->dp_Res2==ERROR_BAD_NUMBER);
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,0,sizeof status)==DOSFALSE);
        CHECK(packet->dp_Res2==ERROR_BAD_NUMBER);
        CHECK(call(XTERM_ACTION_QUERY,9,(IPTR)&status,sizeof status)==DOSFALSE);
        CHECK(packet->dp_Res2==ERROR_INVALID_LOCK);
    }
    CHECK(aros_session_write(&session,(const unsigned char *)"one\r",4)==4);
    {
        XTermStatus status={XTERM_STATUS_VERSION,0,0,0,0,0};
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSTRUE);
        CHECK(status.available==4 && status.eof==0);
    }
    CHECK(call(ACTION_READ,files[0]->fh_Arg1,(IPTR)bytes,sizeof bytes)==4);
    CHECK(!memcmp(bytes,"one\n",4));
    close_input(0); CHECK(!session.eof && aros_session_active(&session));
    CHECK(aros_session_write(&session,(const unsigned char *)"two\r",4)==4);
    CHECK(call(ACTION_READ,files[1]->fh_Arg1,(IPTR)bytes,sizeof bytes)==4);
    CHECK(!memcmp(bytes,"two\n",4));
    lock=(BPTR)call(ACTION_COPY_DIR_FH,files[1]->fh_Arg1,0,0); CHECK(lock);
    close_input(1); CHECK(aros_session_active(&session));
    CHECK(aros_session_dispose(&session)==-1);
    files[2]=AllocDosObject(DOS_FILEHANDLE,NULL); CHECK(files[2]);
    CHECK(call(ACTION_FH_FROM_LOCK,(IPTR)MKBADDR(files[2]),(IPTR)lock,0)==DOSTRUE);
    CHECK(aros_session_write(&session,(const unsigned char *)"end\r",4)==4);
    CHECK(call(ACTION_READ,files[2]->fh_Arg1,(IPTR)bytes,sizeof bytes)==4);
    CHECK(!memcmp(bytes,"end\n",4));
    close_input(2); CHECK(!aros_session_active(&session) && session.exited);
    submit(ACTION_IS_FILESYSTEM,0,0,0);
    CHECK(aros_session_dispose(&session)==-1); CHECK(receive()==DOSFALSE);
    CHECK(!aros_session_dispose(&session));
    puts("PASS endpoint leases: independent handles, lock/reopen, queued disposal");

    {
        XTermStatus status={XTERM_STATUS_VERSION,0,0,0,0,0};
        start(); open_input(0);
        CHECK(call(ACTION_SCREEN_MODE,1,0,0)==DOSTRUE);
        CHECK(aros_session_write(&session,(const unsigned char *)"tail",4)==4);
        aros_session_eof(&session);
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSTRUE);
        CHECK(status.available==4 && status.eof==1);
        CHECK(call(ACTION_READ,files[0]->fh_Arg1,(IPTR)bytes,sizeof bytes)==4);
        CHECK(!memcmp(bytes,"tail",4));
        CHECK(call(XTERM_ACTION_QUERY,files[0]->fh_Arg1,(IPTR)&status,sizeof status)==DOSTRUE);
        CHECK(status.available==0 && status.eof==1);
        CHECK(call(ACTION_READ,files[0]->fh_Arg1,(IPTR)bytes,sizeof bytes)==0);
        close_input(0); CHECK(!aros_session_dispose(&session));
        puts("PASS endpoint query: validation, geometry, non-consuming data, drained EOF");
    }

    /* The 64-packet work budget must not become a lifetime boundary. */
    {
        unsigned i;
        start(); open_input(0);
        files[1]=AllocDosObject(DOS_FILEHANDLE,NULL); CHECK(files[1]);
        for(i=0;i<65;++i) {
            batch[i]=AllocDosObject(DOS_STDPKT,NULL); CHECK(batch[i]);
            batch[i]->dp_Type=i==63?ACTION_END:i==64?ACTION_FINDINPUT:ACTION_IS_FILESYSTEM;
            batch[i]->dp_Arg1=i==63?files[0]->fh_Arg1:i==64?(SIPTR)MKBADDR(files[1]):0;
            SendPkt(batch[i],session.port,reply);
        }
        aros_session_poll(&session);
        CHECK(aros_session_active(&session) && !session.exited && !session.eof);
        aros_session_poll(&session);
        for(i=0;i<65;++i) {
            CHECK(GetMsg(reply)==batch[i]->dp_Link);
            CHECK(batch[i]->dp_Res1==(i>=63?DOSTRUE:DOSFALSE));
            FreeDosObject(DOS_STDPKT,batch[i]); batch[i]=NULL;
        }
        FreeDosObject(DOS_FILEHANDLE,files[0]); files[0]=NULL;
        close_input(1); CHECK(!aros_session_dispose(&session));
        puts("PASS endpoint packet-budget boundary: queued open remains live");
    }

    start(); open_input(0);
    lock=(BPTR)call(ACTION_COPY_DIR_FH,files[0]->fh_Arg1,0,0); CHECK(lock);
    submit(ACTION_READ,files[0]->fh_Arg1,(IPTR)bytes,sizeof bytes);
    aros_session_poll(&session); CHECK(session.pending_read);
    {
        XTermStatus status={XTERM_STATUS_VERSION,0,0,0,0,0};
        struct DosPacket *query=AllocDosObject(DOS_STDPKT,NULL);
        CHECK(query);
        query->dp_Type=XTERM_ACTION_QUERY; query->dp_Arg1=files[0]->fh_Arg1;
        query->dp_Arg2=(IPTR)&status; query->dp_Arg3=sizeof status;
        SendPkt(query,session.port,reply); aros_session_poll(&session);
        CHECK(GetMsg(reply)==query->dp_Link);
        CHECK(query->dp_Res1==DOSFALSE && query->dp_Res2==ERROR_OBJECT_IN_USE);
        FreeDosObject(DOS_STDPKT,query);
    }
    aros_session_stop(&session); CHECK(receive()==0);
    files[1]=AllocDosObject(DOS_FILEHANDLE,NULL); CHECK(files[1]);
    CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(files[1]),0,0)==DOSFALSE);
    CHECK(packet->dp_Res2==ERROR_OBJECT_IN_USE);
    CHECK(call(ACTION_FH_FROM_LOCK,(IPTR)MKBADDR(files[1]),(IPTR)lock,0)==DOSFALSE);
    CHECK(packet->dp_Res2==ERROR_OBJECT_IN_USE);
    CHECK(call(ACTION_FREE_LOCK,(IPTR)lock,0,0)==DOSTRUE);
    FreeDosObject(DOS_FILEHANDLE,files[1]); files[1]=NULL;
    CHECK(call(ACTION_WRITE,files[0]->fh_Arg1,(IPTR)"x",1)==-1);
    CHECK(packet->dp_Res2==ERROR_DEVICE_NOT_MOUNTED);
    close_input(0); CHECK(!aros_session_dispose(&session));
    start(); aros_session_stop(&session); CHECK(!aros_session_dispose(&session));
    puts("PASS endpoint stop: pending EOF, reject new opens/writes, cancel before open");

    start(); open_input(0);
    submit(ACTION_WAIT_CHAR,5000000,0,0);
    aros_session_poll(&session); CHECK(session.wait_count==1 && session.waits[0].active);
    aros_session_stop(&session); CHECK(receive()==DOSTRUE && !session.wait_count && !session.waits[0].active);
    close_input(0); CHECK(!aros_session_dispose(&session));
    {
        static unsigned char burst[XSESSION_CAPACITY+32];
        memset(burst,'x',sizeof burst);
        start(); open_input(0);
        submit(ACTION_WRITE,files[0]->fh_Arg1,(IPTR)burst,sizeof burst);
        aros_session_poll(&session); CHECK(session.pending_write);
        aros_session_stop(&session); CHECK(receive()==-1);
        CHECK(packet->dp_Res2==ERROR_DEVICE_NOT_MOUNTED);
        close_input(0); CHECK(!aros_session_dispose(&session));
    }
    puts("PASS endpoint stop: held lock, timed wait, blocked write");

    /* Three reads and two waiters out at once; the middle read is taken
       back, then the session stops: every packet is answered exactly once
       and nothing stays parked. */
    {
        static char bufs[3][4];
        unsigned i,got=0,seen=0;
        start(); open_input(0);
        for(i=0;i<5;++i) {
            batch[i]=AllocDosObject(DOS_STDPKT,NULL); CHECK(batch[i]);
            batch[i]->dp_Type=i<3?ACTION_READ:ACTION_WAIT_CHAR;
            batch[i]->dp_Arg1=i<3?files[0]->fh_Arg1:5000000;
            batch[i]->dp_Arg2=i<3?(SIPTR)bufs[i]:0;
            batch[i]->dp_Arg3=i<3?(SIPTR)sizeof bufs[i]:0;
            SendPkt(batch[i],session.port,reply);
        }
        aros_session_poll(&session);
        CHECK(session.pending_read==batch[0] && session.read_waiting==2);
        CHECK(session.read_wait[0]==batch[1] && session.read_wait[1]==batch[2]);
        CHECK(session.wait_count==2 && !GetMsg(reply));
        packet->dp_Type=XTY_ACTION_CANCEL; packet->dp_Arg1=files[0]->fh_Arg1;
        packet->dp_Arg2=(SIPTR)batch[1]; packet->dp_Arg3=0;
        packet->dp_Arg4=XTY_PACKET_MAGIC; packet->dp_Arg5=0;
        SendPkt(packet,session.port,reply);
        aros_session_poll(&session);
        CHECK(GetMsg(reply)==batch[1]->dp_Link);
        CHECK(batch[1]->dp_Res1==-1 && batch[1]->dp_Res2==ERROR_BREAK);
        CHECK(GetMsg(reply)==packet->dp_Link && packet->dp_Res1==DOSTRUE);
        CHECK(session.pending_read==batch[0] && session.read_waiting==1 && session.read_wait[0]==batch[2]);
        aros_session_stop(&session);
        for(i=0;i<500 && got<4;++i) {
            struct Message *m;
            unsigned j;
            aros_session_poll(&session);
            while((m=GetMsg(reply))) {
                for(j=0;j<5 && batch[j]->dp_Link!=m;++j) {}
                CHECK(j<5 && j!=1 && !(seen&(1U<<j)));
                seen|=1U<<j; ++got;
                if(j<3) CHECK(batch[j]->dp_Res1==0);
                else CHECK(batch[j]->dp_Res1==DOSTRUE);
            }
            if(got<4) Delay(1);
        }
        CHECK(got==4 && !session.pending_read && !session.read_waiting && !session.wait_count);
        for(i=0;i<XSESSION_WAITERS;++i) CHECK(!session.waits[i].active && !session.waits[i].packet);
        for(i=0;i<5;++i) { FreeDosObject(DOS_STDPKT,batch[i]); batch[i]=NULL; }
        close_input(0); CHECK(!aros_session_dispose(&session));
    }
    puts("PASS endpoint stop: queued reads and waiters answered, cancel of a queued read");

    start(); open_input(0);
    client.fh=MKBADDR(files[0]); client.ok=0;
    process=CreateNewProcTags(NP_Entry,(IPTR)native_client,NP_Name,(IPTR)"XTerm endpoint test client",
        NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE); CHECK(process);
    child_id=GetETaskID(&process->pr_Task); files[0]=NULL;
    Signal(&process->pr_Task,SIGBREAKF_CTRL_F);
    CHECK(aros_session_write(&session,(const unsigned char *)"ok\r",3)==3);
    for(n=0;n<500 && ChildStatus(child_id)!=CHILD_EXITED;++n) {
        aros_session_poll(&session); Delay(1);
    }
    CHECK(ChildStatus(child_id)==CHILD_EXITED);
    ChildFree(child_id); child_id=0;
    CHECK(client.ok && !aros_session_active(&session));
    n=aros_session_read(&session,bytes,sizeof bytes-1); bytes[n]=0;
    CHECK(strstr((const char *)bytes,"NATIVE_WRITE\n"));
    CHECK(!aros_session_dispose(&session));
    FreeDosObject(DOS_STDPKT,packet); DeleteMsgPort(reply);
    puts("AROS external endpoint PASS: native child Read/Write/Close, no owned worker");
    return 0;
}
