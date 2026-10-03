/* Native check of a write cancelled after part of it was queued, through
   posixc write() and through this project's packet client on its own.

   The endpoint replies with the count and ERROR_BREAK (see
   test_aros_tty_break_locks for the packet level). Expected here:
   - "cancel": the endpoint cancels the request (as on hangup). The caller
     gets 1024 of 4096, not -1, so it does not resend the prefix, and no
     break is left pending.
   - "break": the caller's task gets Ctrl-C while blocked. The client cancels
     its own request, still returns 1024, and raises Ctrl-C again so the
     program sees the break (break_pending=1).
   write() uses the client only in the experimental posixc
   (AROS_POSIXC_TTY_EXPERIMENT); with stock posixc the "posix" rows test
   whatever Write() does instead.

   One parent process runs every case ROUNDS times, each with a fresh
   endpoint that is stopped and disposed afterwards, and checks that its own
   allocated signals are the same before and after each round.

   Failure handling: a case that cannot start cleans up what it made. If a
   child has not finished in time the endpoint and the launcher's code are
   still in use, so the test reports it and then waits forever instead of
   freeing, reusing or unloading anything (a runner's timeout reports it).

   Usage: test_posix_partial_write                          (parent)
          test_posix_partial_write child posix|client FILE   (child)
   As in the frontend, a launcher process calls SystemTags: DOS sends
   packets to the PTY during the start, and only the parent's task serves
   them, so the parent must not make that call itself. */
#include "../src/aros_tty_packets.h"
#include "../src/aros_tty_client.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ROUNDS 3
static const char *report_path="RAM:posix-partial-report.txt";

static int child(const char *kind,const char *report)
{
    static char buffer[4096];ssize_t n;int error,client=!strcmp(kind,"client");
    char line[96];BPTR file;ULONG pending;
    memset(buffer,'W',sizeof buffer);
    errno=0;
    if(client) { n=aros_tty_write_break(Output(),buffer,sizeof buffer);error=(int)IoErr(); }
    else { n=write(1,buffer,sizeof buffer);error=errno; }
    /* Read and clear the break at once, before anything else can act on it. */
    pending=SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C;
    snprintf(line,sizeof line,"write=%ld %s=%d break_pending=%d\n",(long)n,client?"ioerr":"errno",error,pending?1:0);
    if((file=Open((CONST_STRPTR)report,MODE_NEWFILE))) { Write(file,line,(LONG)strlen(line));Close(file); }
    return 0;
}

static char command[256];
static volatile int launcher_done;
static void launcher(void)
{
    SystemTags((CONST_STRPTR)command,SYS_Input,Input(),SYS_Output,Output(),NP_StackSize,65536,TAG_END);
    Forbid();launcher_done=1;
}

/* Send a packet to the endpoint and serve it until the reply comes back. */
static int endpoint_call(ArosTtyPackets *e,struct MsgPort *reply,struct DosPacket *p,TtyEnd end,
                         LONG action,SIPTR a1)
{
    unsigned tick;
    p->dp_Type=action;p->dp_Arg1=a1;p->dp_Arg2=p->dp_Arg3=0;
    SendPkt(p,e->ports[end],reply);
    for(tick=0;tick<250;++tick) {
        struct Message *m;
        aros_tty_packets_poll(e);
        if((m=GetMsg(reply))) return m==p->dp_Link;
        Delay(1);
    }
    return 0;
}

/* End every handle still open on the endpoint, stop and dispose it. */
static int teardown(ArosTtyPackets *e,struct FileHandle *master,struct FileHandle *slave)
{
    struct MsgPort *reply=CreateMsgPort();struct DosPacket *p=AllocDosObject(DOS_STDPKT,NULL);
    int disposed=-1;
    aros_tty_packets_stop(e);
    if(reply && p) {
        if(master) endpoint_call(e,reply,p,TTY_MASTER,ACTION_END,master->fh_Arg1);
        if(slave) endpoint_call(e,reply,p,TTY_SLAVE,ACTION_END,slave->fh_Arg1);
    }
    disposed=aros_tty_packets_dispose(e);
    if(p) FreeDosObject(DOS_STDPKT,p);
    if(reply) DeleteMsgPort(reply);
    return disposed;
}

/* One case: 0 expected result, 1 wrong result, -1 cannot continue. */
static int run_case(const char *argv0,const char *kind,int break_mode,int round)
{
    ArosTtyPackets *endpoint;struct FileHandle *master=NULL,*slave=NULL;BPTR nil=0;
    struct DosPacket *pending=NULL;struct Process *proc;
    unsigned tick,i,seen=0;char report[96]={0},expect[64];BPTR file;LONG got;
    unsigned char drained[2048];TtyResult r;int disposed,ok,client=!strcmp(kind,"client");
    ULONG signals_before=FindTask(NULL)->tc_SigAlloc,signals_after;

    /* A fresh endpoint per case: nothing from an earlier case is reused. */
    endpoint=AllocVec(sizeof *endpoint,MEMF_CLEAR);
    DeleteFile((CONST_STRPTR)report_path);launcher_done=0;
    if(endpoint) { master=AllocDosObject(DOS_FILEHANDLE,NULL);slave=AllocDosObject(DOS_FILEHANDLE,NULL); }
    nil=Open((CONST_STRPTR)"NIL:",MODE_OLDFILE);
    if(!endpoint || !master || !slave || !nil || aros_tty_packets_init(endpoint,master,slave)) {
        if(nil) Close(nil);
        if(slave) FreeDosObject(DOS_FILEHANDLE,slave);
        if(master) FreeDosObject(DOS_FILEHANDLE,master);
        if(endpoint) FreeVec(endpoint);
        puts("POSIX PARTIAL SETUP FAIL");return 1;
    }
    snprintf(command,sizeof command,"\"%s\" child %s %s",argv0,kind,report_path);
    /* On success the launcher owns nil and the slave handle and closes them. */
    proc=CreateNewProcTags(NP_Entry,(IPTR)launcher,NP_Name,(IPTR)"posix partial launcher",
        NP_StackSize,65536,NP_Cli,TRUE,NP_Input,nil,NP_Output,MKBADDR(slave),
        NP_CloseInput,TRUE,NP_CloseOutput,TRUE,TAG_DONE);
    if(!proc) {
        Close(nil);
        disposed=teardown(endpoint,master,slave);
        FreeDosObject(DOS_FILEHANDLE,master);
        /* The slave handle was never given away; the endpoint no longer uses it. */
        FreeDosObject(DOS_FILEHANDLE,slave);
        if(!disposed) FreeVec(endpoint);
        puts("POSIX PARTIAL START FAIL");return 1;
    }
    for(tick=0;tick<1000;++tick) {
        aros_tty_packets_poll(endpoint);
        if(!pending) {
            for(i=0;i<TTY_SERVICE_REQUESTS;++i)
                if(endpoint->slots[i].packet && endpoint->slots[i].request.operation==TTY_REQUEST_WRITE)
                    pending=endpoint->slots[i].packet;
        } else if(++seen==25) {
            /* Pending for half a second with 1024 of 4096 bytes queued. */
            if(break_mode) Signal(pending->dp_Port->mp_SigTask,SIGBREAKF_CTRL_C);
            else aros_tty_packets_cancel(endpoint,pending);
            aros_tty_packets_poll(endpoint);
        }
        if(seen>25 && !report[0] && (file=Open((CONST_STRPTR)report_path,MODE_OLDFILE))) {
            got=Read(file,report,sizeof report-1);Close(file);
            if(got>0) report[got]=0;
        }
        if(report[0] && launcher_done) break;
        Delay(1);
    }
    /* Give a slow child more time, still serving its packets. */
    for(i=0;!launcher_done && i<1500;++i) { aros_tty_packets_poll(endpoint);Delay(1); }
    if(!launcher_done) {
        printf("FAIL %-6s %-6s round %d: child did not finish; stopping here, nothing freed\n",
               kind,break_mode?"break":"cancel",round);
        /* The launcher runs code in this program and uses the endpoint.
           Freeing, reusing or returning (unloading) would be unsafe. */
        for(;;) { aros_tty_packets_poll(endpoint);Delay(50); }
    }
    /* Serve the launcher's closing packets; it ended inside Forbid. */
    for(i=0;i<25;++i) { aros_tty_packets_poll(endpoint);Delay(1); }
    r=tty_pair_read(&endpoint->service.pair,TTY_MASTER,drained,sizeof drained);
    /* DOS closed the slave handle with the launcher; end the master too. */
    disposed=teardown(endpoint,master,NULL);
    FreeDosObject(DOS_FILEHANDLE,master);
    if(!disposed) FreeVec(endpoint);
    signals_after=FindTask(NULL)->tc_SigAlloc;

    snprintf(expect,sizeof expect,"write=1024 %s break_pending=%d",client?"ioerr=304":"errno=0",break_mode);
    ok=tick<1000 && !strncmp(report,expect,strlen(expect)) && r.count==1024 && !disposed &&
       signals_after==signals_before;
    printf("%s %-6s %-6s round %d: master=%lu disposed=%d sigalloc %08lx->%08lx %s",
           ok?"PASS":"FAIL",kind,break_mode?"break":"cancel",round,(unsigned long)r.count,disposed,
           (unsigned long)signals_before,(unsigned long)signals_after,report[0]?report:"(no report)\n");
    return !ok;
}

int main(int argc,char **argv)
{
    int round,failed=0;
    if(argc>3 && !strcmp(argv[1],"child")) return child(argv[2],argv[3]);
    for(round=1;round<=ROUNDS;++round) {
        failed+=run_case(argv[0],"client",0,round);
        failed+=run_case(argv[0],"posix",0,round);
        failed+=run_case(argv[0],"client",1,round);
        failed+=run_case(argv[0],"posix",1,round);
    }
    printf("POSIX PARTIAL %s: %d of %d cases failed\n",failed?"FAIL":"PASS",failed,ROUNDS*4);
    return failed?5:0;
}
