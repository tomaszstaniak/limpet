/* Run beside xterm-handler and the HEADLESS aros-xterm-worker test binary. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/arossupport.h>
#include <dos/dostags.h>
#include <dos/filehandler.h>
#include <dos/stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static BPTR segment,handles[2];
static ULONG children[2];
static struct MsgPort *reply;
static struct DosPacket *packet;
static int outstanding;
static struct DeviceNode node;
static BSTR handler,path;

static void join(unsigned i)
{
    if(children[i]) {
        while(ChildStatus(children[i])!=CHILD_EXITED) Delay(1);
        ChildFree(children[i]); children[i]=0;
    }
}
static void fail(const char *what,int line)
{
    unsigned i;
    printf("FAIL launcher %d: %s\n",line,what); fflush(stdout);
    /* Do not free packet storage or cached code still referenced by a child.
     * A broken startup protocol deliberately parks the harness for diagnosis. */
    if(outstanding) for(;;) Delay(50);
    for(i=0;i<2;++i) if(handles[i]) { Close(handles[i]); handles[i]=0; }
    for(i=0;i<2;++i) join(i);
    if(segment) UnLoadSeg(segment);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static SIPTR exchange(struct MsgPort *port,LONG action,IPTR a,IPTR b,IPTR c)
{
    unsigned tick;
    packet->dp_Type=action; packet->dp_Arg1=a; packet->dp_Arg2=b; packet->dp_Arg3=c;
    outstanding=1; SendPkt(packet,port,reply);
    for(tick=0;tick<1000;++tick) {
        struct Message *m=GetMsg(reply);
        if(m) { CHECK(m==packet->dp_Link); outstanding=0; return packet->dp_Res1; }
        Delay(1);
    }
    fail("packet deadline (child retained)",__LINE__); return 0;
}
static struct Process *start(unsigned i)
{
    struct Process *p=CreateNewProcTags(NP_Entry,(IPTR)BADDR(segment)+sizeof(IPTR),
        NP_Seglist,segment,NP_FreeSeglist,FALSE,NP_NotifyOnDeath,TRUE,
        NP_Name,(IPTR)"xterm launcher test",NP_StackSize,65536,TAG_DONE);
    CHECK(p); children[i]=GetETask(&p->pr_Task)->et_UniqueID;
    return p;
}
int main(void)
{
    char name[1024],bytes[8];
    unsigned i;
    struct Process *p;
    CHECK(NameFromLock(GetProgramDir(),(STRPTR)name,sizeof name));
    CHECK(AddPart((STRPTR)name,(CONST_STRPTR)"xterm-handler",sizeof name));
    segment=LoadSeg((CONST_STRPTR)name); CHECK(segment);
    handler=CreateBSTR((CONST_STRPTR)name); path=CreateBSTR((CONST_STRPTR)"XTERM:");
    reply=CreateMsgPort(); packet=AllocDosObject(DOS_STDPKT,NULL);
    CHECK(handler && path && reply && packet);
    node.dn_Handler=handler; node.dn_GlobalVec=(BPTR)-1;
    for(i=0;i<2;++i) {
        struct FileHandle *fh=AllocDosObject(DOS_FILEHANDLE,NULL);
        /* Separate startup path allocation; both use the same cached segment. */
        BSTR client_path=CreateBSTR((CONST_STRPTR)(i?"XTERM2:":"XTERM1:"));
        struct DeviceNode client_node=node;
        CHECK(client_path);
        CHECK(fh); p=start(i);
        CHECK(exchange(&p->pr_MsgPort,0,(IPTR)client_path,0,(IPTR)MKBADDR(&client_node))==DOSTRUE);
        CHECK(exchange(&p->pr_MsgPort,ACTION_FINDUPDATE,(IPTR)MKBADDR(fh),0,(IPTR)client_path)==DOSTRUE);
        FreeVec(BADDR(client_path));
        handles[i]=MKBADDR(fh);
        CHECK(SetVBuf(handles[i],NULL,BUF_NONE,-1)==0);
    }
    CHECK(((struct FileHandle *)BADDR(handles[0]))->fh_Type!=
          ((struct FileHandle *)BADDR(handles[1]))->fh_Type);
    CHECK(Write(handles[0],"ONE",3)==3); CHECK(Write(handles[1],"TWO",3)==3);
    CHECK(Read(handles[0],bytes,3)==3 && !memcmp(bytes,"ONE",3));
    CHECK(Read(handles[1],bytes,3)==3 && !memcmp(bytes,"TWO",3));
    CHECK(Close(handles[0])); handles[0]=0; join(0);
    CHECK(Write(handles[1],"LIVE",4)==4);
    CHECK(Read(handles[1],bytes,4)==4 && !memcmp(bytes,"LIVE",4));
    CHECK(Close(handles[1])); handles[1]=0; join(1);
    puts("PASS launcher: cached entry, private workers, native I/O isolation, independent exit");
    FreeVec(BADDR(path)); path=CreateBSTR((CONST_STRPTR)"XTERM:unsupported"); CHECK(path);
    p=start(0);
    CHECK(exchange(&p->pr_MsgPort,0,(IPTR)path,0,(IPTR)MKBADDR(&node))==DOSFALSE);
    CHECK(packet->dp_Res2==ERROR_BAD_NUMBER); join(0);
    CHECK(!GetMsg(reply));
    puts("PASS launcher: unsupported suffix rejected and worker joined");
    FreeVec(BADDR(handler)); handler=CreateBSTR((CONST_STRPTR)"RAM:no-such-xterm-dir/xterm-handler");
    CHECK(handler); node.dn_Handler=handler;
    p=start(0);
    CHECK(exchange(&p->pr_MsgPort,0,(IPTR)path,0,(IPTR)MKBADDR(&node))==DOSFALSE);
    CHECK(packet->dp_Res2!=0); join(0); CHECK(!GetMsg(reply));
    puts("PASS launcher: missing directory replies once and exits");
    /* Existing directory, but no sibling worker: exercise LoadSeg failure. */
    {
        BPTR lock;
        snprintf(name,sizeof name,"RAM:xterm-loader-empty-%lu",(unsigned long)
                 GetETask(FindTask(NULL))->et_UniqueID);
        lock=CreateDir((CONST_STRPTR)name); CHECK(lock); UnLock(lock);
        CHECK(AddPart((STRPTR)name,(CONST_STRPTR)"xterm-handler",sizeof name));
        FreeVec(BADDR(handler)); handler=CreateBSTR((CONST_STRPTR)name);
        CHECK(handler); node.dn_Handler=handler;
        p=start(0);
        CHECK(exchange(&p->pr_MsgPort,0,(IPTR)path,0,(IPTR)MKBADDR(&node))==DOSFALSE);
        CHECK(packet->dp_Res2!=0); join(0); CHECK(!GetMsg(reply));
        *(char *)FilePart((CONST_STRPTR)name)=0;
        CHECK(DeleteFile((CONST_STRPTR)name));
        puts("PASS launcher: missing worker replies once and exits");
    }
    FreeDosObject(DOS_STDPKT,packet); DeleteMsgPort(reply);
    FreeVec(BADDR(handler)); FreeVec(BADDR(path)); UnLoadSeg(segment);
    puts("DOS launcher tests PASS"); return 0;
}
