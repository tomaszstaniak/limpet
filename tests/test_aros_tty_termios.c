#include "../src/aros_tty_termios.h"
#include "../src/aros_tty_worker.h"
#include "../src/tty_control.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ArosTtyWorker *worker;
static BPTR master,slave;
static ULONG child_id;
static struct FileHandle *proxy;
static struct MsgPort *proxy_port,*forward_reply;
static struct DosPacket *forward_packet,*original;
static struct termios child_config;
static unsigned child_operation;
static int child_result,child_error;
static unsigned corruptions;
static int child_preserved;
static int collect_forward(void)
{
    struct Message *m=GetMsg(forward_reply);struct DosPacket *p;
    if(!m) return 0;
    if(child_operation>=3 && forward_packet->dp_Type==XTY_ACTION_CONTROL &&
       forward_packet->dp_Res1==DOSTRUE) {
        XtyControl c;uint32_t target=child_operation==7?XTY_CAPS:child_operation==3?XTY_GET_CONFIG:
            child_operation==4?XTY_SET_CONFIG:child_operation==5?XTY_DRAIN:XTY_FLUSH;
        memcpy(&c,(void *)forward_packet->dp_Arg2,sizeof c);
        if(c.operation==target) {
            /* Controlled malformed-success responder; transport and server
               remain real. Test the client's distrust of returned values. */
            if(child_operation==7) c.values[0]&=~XTY_CAP_CANCEL;
            else if(child_operation==4) c.values[3]=5;
            else c.values[0]=child_operation==3?8:1;
            memcpy((void *)forward_packet->dp_Arg2,&c,sizeof c);++corruptions;
        }
    }
    p=original;original=NULL;
    ReplyPkt(p,forward_packet->dp_Res1,forward_packet->dp_Res2);
    return 1;
}
static void fail(const char *expr,int line)
{
    printf("TTY TERMIOS NATIVE FAIL line=%d: %s\n",line,expr);fflush(stdout);
    if(worker) aros_tty_worker_stop(worker);
    while(child_id && ChildStatus(child_id)!=CHILD_EXITED) {
        struct Message *m;
        if(original) collect_forward();
        while(proxy_port && (m=GetMsg(proxy_port)))
            ReplyPkt((struct DosPacket *)m->mn_Node.ln_Name,DOSFALSE,ERROR_BREAK);
        Delay(1);
    }
    if(child_id) ChildFree(child_id);
    if(master) Close(master);
    if(slave) Close(slave);
    if(worker && aros_tty_worker_finish(worker)) for(;;) Delay(50);
    if(proxy) FreeDosObject(DOS_FILEHANDLE,proxy);
    if(forward_packet) FreeDosObject(DOS_STDPKT,forward_packet);
    if(proxy_port) DeleteMsgPort(proxy_port);
    if(forward_reply) DeleteMsgPort(forward_reply);
    exit(20);
}
#define CHECK(e) do { if(!(e)) fail(#e,__LINE__); } while(0)
static void client(void)
{
    BPTR handle=MKBADDR(proxy);LONG written;
    if(child_operation>=3) {
        struct termios t,before;
        memset(&t,0xa5,sizeof t);before=t;
        if(child_operation==3) child_result=aros_tty_tcgetattr(handle,&t);
        else if(child_operation==4) child_result=aros_tty_tcsetattr(handle,TCSANOW,&child_config);
        else if(child_operation==5 || child_operation==7) child_result=aros_tty_tcdrain(handle);
        else child_result=aros_tty_tcflush(handle,TCIOFLUSH);
        child_error=errno;child_preserved=!memcmp(&t,&before,sizeof t);return;
    }
    written=Write(handle,"old",3);
    if(written!=3) { child_result=-2;return; }
    child_result=child_operation==0?aros_tty_tcdrain(handle):
        aros_tty_tcsetattr(handle,child_operation==1?TCSADRAIN:TCSAFLUSH,&child_config);
    child_error=errno;
}
int main(void)
{
    struct termios raw,t,got,before;unsigned cycle,tick;unsigned char b[16];BPTR nil;
    worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
    proxy_port=CreateMsgPort();forward_reply=CreateMsgPort();
    proxy=AllocDosObject(DOS_FILEHANDLE,NULL);forward_packet=AllocDosObject(DOS_STDPKT,NULL);
    CHECK(proxy_port && forward_reply && proxy && forward_packet);
    proxy->fh_Type=proxy_port;proxy->fh_Arg1=((struct FileHandle *)BADDR(slave))->fh_Arg1;
    proxy->fh_Interactive=DOSTRUE;
    memset(&raw,0xa5,sizeof raw);CHECK(!aros_tty_tcgetattr(slave,&raw));
    CHECK(!raw.c_iflag && !raw.c_oflag && !raw.c_lflag && raw.c_cc[VEOF]==4);
    CHECK(raw.c_cc[VMIN]==1 && !raw.c_cc[VTIME]);
    for(cycle=0;cycle<100;++cycle) {
        CHECK(!aros_tty_tcflow(slave,TCOOFF));CHECK(!aros_tty_tcflow(slave,TCOOFF));
        CHECK(Write(slave,"held",4)==4);CHECK(WaitForChar(master,0)==DOSFALSE);
        CHECK(Read(master,NULL,0)==0);
        CHECK(!aros_tty_tcflow(slave,TCOON));
        CHECK(Read(master,b,sizeof b)==4 && !memcmp(b,"held",4));
    }
    CHECK(aros_tty_tcflow(master,TCOOFF)==-1 && errno==ENOTSUP);
    CHECK(aros_tty_tcflow(slave,TCIOFF)==-1 && errno==ENOTSUP);
    CHECK(aros_tty_tcflow(slave,TCION)==-1 && errno==ENOTSUP);
    puts("TTY OUTPUT FLOW NATIVE PASS: cycles=100 stop/restart, availability, empty read, unsupported input flow");fflush(stdout);
    for(cycle=0;cycle<100;++cycle) {
        t=raw;t.c_lflag=ICANON|ECHO|ECHOE|ECHOK;t.c_oflag=OPOST|ONLCR;
        t.c_cc[VERASE]=127;t.c_cc[VKILL]=21;t.c_cc[VEOL]=';';t.c_cc[VEOF]=(cc_t)_POSIX_VDISABLE;
        CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
        CHECK(!aros_tty_tcgetattr(master,&got));
        CHECK(got.c_lflag==t.c_lflag && !memcmp(got.c_cc,t.c_cc,sizeof t.c_cc));
        CHECK(Write(master,"ab\177c;",5)==5);
        CHECK(Read(slave,b,sizeof b)==3 && !memcmp(b,"ac;",3));
        CHECK(Read(master,b,sizeof b)==7 && !memcmp(b,"ab\b \bc;",7));
        CHECK(Write(master,"bad\025\004\n",6)==6);
        CHECK(Read(slave,b,sizeof b)==2 && !memcmp(b,"\004\n",2));
        CHECK(Read(master,b,sizeof b)==9 && !memcmp(b,"bad\025\r\n\004\r\n",9));
        CHECK(!aros_tty_tcsetattr(slave,TCSADRAIN,&raw));
    }
    puts("TTY CANONICAL TERMIOS NATIVE PASS: cycles=100 erase/kill/EOL, disabled EOF, CRLF echo, shared snapshots");fflush(stdout);
    t=raw;t.c_iflag=ICRNL;t.c_lflag=ICANON|ECHO;
    CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&t));
    CHECK(!aros_tty_tcgetattr(master,&got) && got.c_lflag==(ICANON|ECHO));
    CHECK(Write(master,"a\r",2)==2);
    CHECK(Read(slave,b,sizeof b)==2 && !memcmp(b,"a\n",2));
    CHECK(Read(master,b,sizeof b)==2 && !memcmp(b,"a\n",2));
    CHECK(Write(master,"\004",1)==1 && Read(slave,b,sizeof b)==0);
    CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&raw));
    t=raw;t.c_lflag=ISIG;CHECK(aros_tty_tcsetattr(slave,TCSAFLUSH,&t)==-1 && errno==ENOTSUP);
    CHECK(!aros_tty_tcgetattr(slave,&got) && !got.c_lflag);
    CHECK(aros_tty_tcsetattr(master,TCSANOW,&raw)==-1 && errno==ENOTSUP);
    CHECK(aros_tty_tcflush(master,TCIFLUSH)==-1 && errno==ENOTSUP);
    CHECK(aros_tty_tcdrain(master)==-1 && errno==ENOTSUP);
    CHECK(aros_tty_tcgetattr(BNULL,&got)==-1 && errno==EBADF);
    CHECK(aros_tty_tcgetattr(slave,NULL)==-1 && errno==EFAULT);
    before=got;nil=Open((CONST_STRPTR)"NIL:",MODE_NEWFILE);CHECK(nil);
    CHECK(aros_tty_tcgetattr(nil,&got)==-1 && errno==ENOTTY);
    CHECK(!memcmp(&got,&before,sizeof got));CHECK(Close(nil));
    CHECK(Write(master,"keep",4)==4);
    t=raw;t.c_oflag=OPOST|ONLCR;
    CHECK(!aros_tty_tcsetattr(slave,TCSADRAIN,&t));
    CHECK(!aros_tty_tcgetattr(slave,&got) && got.c_oflag==(OPOST|ONLCR));
    CHECK(Read(slave,b,sizeof b)==4 && !memcmp(b,"keep",4));
    for(cycle=0;cycle<29;++cycle) {
        struct Process *p;unsigned observed=0;int written=0;
        CHECK(!aros_tty_tcsetattr(slave,TCSANOW,&raw));
        child_operation=cycle<24?cycle%3:cycle-21;child_config=raw;child_config.c_oflag=OPOST|ONLCR;
        child_result=-99;child_error=0;child_preserved=0;corruptions=0;
        if(child_operation==2) CHECK(Write(master,"stale",5)==5);
        p=CreateNewProcTags(NP_Entry,(IPTR)client,NP_Name,(IPTR)"Termios client",
            NP_StackSize,65536,NP_NotifyOnDeath,TRUE,TAG_DONE);CHECK(p);
        child_id=GetETaskID(&p->pr_Task);CHECK(child_id);
        /* A transparent packet relay observes the real client's operation,
           forwards it unchanged to the real worker, and retains its reply.
           No mocked capabilities or simulated drain semantics are involved. */
        for(tick=0;tick<500 && ChildStatus(child_id)!=CHILD_EXITED;++tick) {
            struct Message *m;
            if(original && forward_packet->dp_Type==ACTION_WRITE) {
                if(collect_forward()) written=forward_packet->dp_Res1==3 && !forward_packet->dp_Res2;
            } else if(original) collect_forward();
            if(!original && (m=GetMsg(proxy_port))) {
                struct MsgPort *port=((struct FileHandle *)BADDR(slave))->fh_Type;
                XtyControl c;int barrier=0;
                original=(struct DosPacket *)m->mn_Node.ln_Name;
                forward_packet->dp_Type=original->dp_Type;
                forward_packet->dp_Arg1=original->dp_Arg1;forward_packet->dp_Arg2=original->dp_Arg2;
                forward_packet->dp_Arg3=original->dp_Arg3;forward_packet->dp_Arg4=original->dp_Arg4;
                forward_packet->dp_Arg5=original->dp_Arg5;
                if(original->dp_Type==XTY_ACTION_CONTROL) {
                    memcpy(&c,(void *)original->dp_Arg2,sizeof c);
                    barrier=c.operation==XTY_DRAIN || c.operation==XTY_DRAIN_CONFIG ||
                        c.operation==XTY_DRAIN_CONFIG_DISCARD_INPUT;
                }
                SendPkt(forward_packet,port,forward_reply);
                if(barrier && child_operation<3) {
                    CHECK(written);++observed;
                    CHECK(DoPkt(port,ACTION_IS_FILESYSTEM,0,0,0,0,0)==DOSFALSE);
                    CHECK(!collect_forward());
                    CHECK(WaitForChar(master,1000000)==DOSTRUE);
                    CHECK(Read(master,b,2)==2 && !memcmp(b,"ol",2));
                    CHECK(!collect_forward());
                    CHECK(WaitForChar(master,1000000)==DOSTRUE);
                    CHECK(Read(master,b,1)==1 && b[0]=='d');
                }
            }
            Delay(1);
        }
        CHECK(ChildStatus(child_id)==CHILD_EXITED);
        ChildFree(child_id);child_id=0;CHECK(!original);
        if(child_operation>=3) {
            CHECK(child_result==-1 && child_error==(child_operation==7?ENOTSUP:EIO) && corruptions==1);
            if(child_operation==3) CHECK(child_preserved);
            continue;
        }
        CHECK(child_result==0 && observed==1);
        CHECK(!aros_tty_tcgetattr(slave,&got));
        CHECK(got.c_oflag==(child_operation?OPOST|ONLCR:0));
        CHECK(WaitForChar(slave,0)==DOSFALSE);
        CHECK(Write(slave,"\n",1)==1);
        CHECK(Read(master,b,sizeof b)==(child_operation?2:1));
        CHECK(!memcmp(b,child_operation?"\r\n":"\n",child_operation?2:1));
    }
    CHECK(Write(master,"input",5)==5 && Write(slave,"output",6)==6);
    CHECK(!aros_tty_tcflush(slave,TCIFLUSH));CHECK(WaitForChar(slave,0)==DOSFALSE);
    CHECK(Read(master,b,sizeof b)==6 && !memcmp(b,"output",6));
    CHECK(Write(master,"input",5)==5 && Write(slave,"output",6)==6);
    CHECK(!aros_tty_tcflush(slave,TCOFLUSH));CHECK(WaitForChar(master,0)==DOSFALSE);
    CHECK(Read(slave,b,sizeof b)==5 && !memcmp(b,"input",5));
    CHECK(Write(master,"input",5)==5 && Write(slave,"output",6)==6);
    CHECK(!aros_tty_tcflush(slave,TCIOFLUSH));
    CHECK(WaitForChar(slave,0)==DOSFALSE && WaitForChar(master,0)==DOSFALSE);
    CHECK(!aros_tty_tcdrain(slave));
    CHECK(!aros_tty_worker_stop(worker));
    CHECK(aros_tty_tcdrain(slave)==-1 && errno==EBADF);
    CHECK(Close(master));master=BNULL;CHECK(Close(slave));slave=BNULL;
    CHECK(!aros_tty_worker_finish(worker));worker=NULL;
    FreeDosObject(DOS_FILEHANDLE,proxy);proxy=NULL;
    FreeDosObject(DOS_STDPKT,forward_packet);forward_packet=NULL;
    DeleteMsgPort(proxy_port);proxy_port=NULL;DeleteMsgPort(forward_reply);forward_reply=NULL;
    puts("TTY TERMIOS NATIVE PASS: child_cycles=24 actions=3 malformed_success=4 flags/EOF, shared snapshot, capability rejection, input preservation, flush=3, stop/reap");
    return 0;
}
