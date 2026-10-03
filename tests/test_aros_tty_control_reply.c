/* Faulty native responder; never writes outside the supplied control block. */
#include "../src/aros_tty_client.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <string.h>

static struct FileHandle *handle;
static XtyControl output,before;
static LONG result,error;
static unsigned blocking;
static void client(void)
{
    result=blocking?aros_tty_control_break(MKBADDR(handle),&output):
                    aros_tty_control(MKBADDR(handle),&output);
    error=IoErr();
}
int main(void)
{
    struct MsgPort *port=CreateMsgPort();unsigned test,passed=0;
    handle=AllocDosObject(DOS_FILEHANDLE,NULL);
    if(!port || !handle) return 20;
    handle->fh_Type=port;handle->fh_Arg1=7;handle->fh_Interactive=DOSTRUE;
    for(blocking=0;blocking<2;++blocking) for(test=0;test<10;++test) {
        struct Process *child;ULONG id;unsigned tick,replies=0;
        memset(&output,0,sizeof output);output.version=XTY_CONTROL_VERSION;
        output.operation=XTY_GET_CONFIG;before=output;result=42;error=0;
        child=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"control reply probe",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);
        if(!child) return 20;
        id=GetETaskID(&child->pr_Task);
        if(!id) { puts("CONTROL REPLY FAIL: missing child id");fflush(stdout);for(;;) Delay(50); }
        for(tick=0;tick<250 && ChildStatus(id)!=CHILD_EXITED;++tick) {
            struct Message *m;
            while((m=GetMsg(port))) {
                struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
                XtyControl *c=(XtyControl *)p->dp_Arg2;
                if(p->dp_Type!=XTY_ACTION_CONTROL || p->dp_Arg3!=sizeof *c) {
                    ReplyPkt(p,DOSFALSE,ERROR_ACTION_NOT_KNOWN);continue;
                }
                c->reserved[0]=XTY_PACKET_MAGIC;c->values[0]=42;++replies;
                if(test==4) c->reserved[0]=0;
                if(test==5) c->operation=XTY_CAPS;
                if(test==6) c->reserved[1]=1;
                if(test==7) c->version=2;
                /* ReplyPkt takes LONG res2 and would truncate the very fault
                   being injected. Return the raw packet at its actual width. */
                {
                    struct MsgPort *destination=p->dp_Port;
                    p->dp_Res1=test==1?(SIPTR)UINT64_C(0xffffffff):test==3?DOSFALSE:DOSTRUE;
                    p->dp_Res2=test==8?(SIPTR)INT64_C(0x100000000):test==9?-1:
                        test==2 || test==3?ERROR_BREAK:0;
                    p->dp_Port=port;
                    PutMsg(destination,p->dp_Link);
                }
            }
            Delay(1);
        }
        if(ChildStatus(id)!=CHILD_EXITED) {
            printf("CONTROL REPLY FAIL: timeout blocking=%u case=%u\n",blocking,test);fflush(stdout);
            /* External watchdog owns this VM; do not free live client code. */
            for(;;) Delay(50);
        }
        ChildFree(id);
        if(replies!=1 || (!test?(result!=DOSTRUE || error || output.values[0]!=42):
           (result!=DOSFALSE || !error || memcmp(&output,&before,sizeof output)))) {
            printf("CONTROL REPLY FAIL: blocking=%u case=%u result=%ld error=%ld\n",blocking,test,(long)result,(long)error);
            FreeDosObject(DOS_FILEHANDLE,handle);DeleteMsgPort(port);return 20;
        }
        ++passed;
    }
    FreeDosObject(DOS_FILEHANDLE,handle);DeleteMsgPort(port);
    printf("CONTROL REPLY PASS: cases=%u immediate/draining, wide result, contradictory success, headers, unchanged failure\n",passed);
    return 0;
}
