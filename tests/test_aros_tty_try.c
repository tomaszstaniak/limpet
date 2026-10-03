#include "../src/aros_tty_worker.h"
#include "../src/aros_tty_client.h"
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

static ArosTtyWorker *worker;
static BPTR master,slave,copy;
static int failed;
#define CHECK(e) do { if(!(e)) { failed=__LINE__;goto done; } } while(0)
static LONG wire(BPTR h,LONG action,SIPTR id,void *b,SIPTR n,SIPTR magic,SIPTR reserved)
{
    struct FileHandle *fh=BADDR(h);
    return DoPkt(fh->fh_Type,action,id,(SIPTR)b,n,magic,reserved);
}
int main(void)
{
    unsigned cycle,end;char b[TTY_QUEUE_CAPACITY],fill[TTY_QUEUE_CAPACITY];
    TtyConfig config;XtyControl caps;BPTR nil;
    memset(fill,'q',sizeof fill);
    for(cycle=1;cycle<=10;++cycle) {
        worker=aros_tty_worker_start(&master,&slave);CHECK(worker);
        for(end=0;end<2;++end) {
            memset(&caps,0,sizeof caps);caps.version=1;caps.operation=XTY_CAPS;
            CHECK(aros_tty_control(end?slave:master,&caps)==DOSTRUE);
            CHECK((caps.values[0]&(XTY_CAP_TRY_IO|XTY_CAP_CANCEL))==(XTY_CAP_TRY_IO|XTY_CAP_CANCEL));
        }
        CHECK(aros_tty_worker_duplicate(worker,slave,&copy)==TTY_OK);
        CHECK(aros_tty_worker_get_config(worker,1,&config)==TTY_OK);
        config.vmin=3;config.vtime=2;
        CHECK(aros_tty_worker_configure(worker,&config)==TTY_OK);
        memset(b,'x',sizeof b);
        CHECK(aros_tty_read_try(slave,b,8)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(!memcmp(b,"xxxxxxxx",8));
        CHECK(aros_tty_write_try(master,"A\0",2)==2);
        CHECK(aros_tty_read_try(copy,b,8)==2 && !memcmp(b,"A\0xxxxxx",8));
        CHECK(aros_tty_write_try(slave,fill,sizeof fill-1)==sizeof fill-1);
        CHECK(aros_tty_write_try(copy,"yz",2)==1);
        CHECK(aros_tty_write_try(slave,"BAD",3)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(aros_tty_read_try(master,b,sizeof b)==sizeof b);
        CHECK(!memcmp(b,fill,sizeof b-1) && b[sizeof b-1]=='y');
        CHECK(aros_tty_read_try(master,b,1)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(aros_tty_write_try(copy,"z",1)==1);
        CHECK(aros_tty_read_try(master,b,1)==1 && b[0]=='z');
        Signal(FindTask(NULL),SIGBREAKF_CTRL_C);
        CHECK(aros_tty_read_try(slave,NULL,0)==0);
        CHECK(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C);
        Signal(FindTask(NULL),SIGBREAKF_CTRL_C);
        CHECK(aros_tty_write_try(slave,NULL,0)==0);
        CHECK(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C);
        CHECK(aros_tty_read_try(slave,NULL,1)==-1 && IoErr()==ERROR_BAD_NUMBER);
        CHECK(aros_tty_write_try(slave,"x",-1)==-1 && IoErr()==ERROR_BAD_NUMBER);
        memset(b,'x',8);
        CHECK(aros_tty_write_try(master,"keep",4)==4);
        for(end=0;end<2;++end) {
            LONG action=end?XTY_ACTION_TRY_WRITE:XTY_ACTION_TRY_READ;
            SIPTR id=((struct FileHandle *)BADDR(slave))->fh_Arg1;
            CHECK(wire(slave,action,id,b,1,0,0)==-1 && IoErr()==ERROR_BAD_NUMBER);
            CHECK(wire(slave,action,id,b,1,XTY_PACKET_MAGIC,1)==-1 && IoErr()==ERROR_BAD_NUMBER);
            CHECK(wire(slave,action,id,b,(SIPTR)INT32_MAX+1,XTY_PACKET_MAGIC,0)==-1 && IoErr()==ERROR_BAD_NUMBER);
            CHECK(wire(master,action,id,b,1,XTY_PACKET_MAGIC,0)==-1 && IoErr()==ERROR_INVALID_LOCK);
            CHECK(wire(slave,action,0,b,1,XTY_PACKET_MAGIC,0)==-1 && IoErr()==ERROR_INVALID_LOCK);
            CHECK(!memcmp(b,"xxxxxxxx",8));
        }
        CHECK(aros_tty_read_try(slave,b,8)==4 && !memcmp(b,"keep",4));
        CHECK(aros_tty_read_try(master,b,8)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        config.local_flags=TTY_CANON;CHECK(aros_tty_worker_configure(worker,&config)==TTY_OK);
        CHECK(aros_tty_write_try(master,"ab",2)==2);
        CHECK(aros_tty_read_try(slave,b,8)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(aros_tty_write_try(master,"\n",1)==1);
        CHECK(aros_tty_read_try(slave,b,8)==3 && !memcmp(b,"ab\n",3));
        CHECK(aros_tty_write_try(master,"\4",1)==1);
        CHECK(aros_tty_read_try(slave,b,8)==0 && !IoErr());
        CHECK(aros_tty_read_try(slave,b,8)==-1 && IoErr()==ERROR_OBJECT_IN_USE);
        CHECK(aros_tty_write_try(master,"c\n",2)==2);
        CHECK(aros_tty_read_try(slave,b,8)==2 && !memcmp(b,"c\n",2));
        CHECK(Close(master));master=0;
        CHECK(aros_tty_read_try(copy,b,8)==0 && !IoErr());
        CHECK(aros_tty_write_try(copy,"x",1)==-1 && IoErr()==ERROR_INVALID_LOCK);
        CHECK(Close(copy));copy=0;CHECK(Close(slave));slave=0;
        CHECK(aros_tty_worker_finish(worker)==0);worker=NULL;
        printf("TTY TRY WIRE CYCLE PASS: %u binary short/full, zero signal, validation, EOF/restart, cleanup\n",cycle);
    }
    nil=Open((CONST_STRPTR)"NIL:",MODE_OLDFILE);CHECK(nil);
    { LONG n=aros_tty_read_try(nil,b,1),error=IoErr();Close(nil);
      CHECK(n==-1 && error==ERROR_ACTION_NOT_KNOWN); }
done:
    if(worker) aros_tty_worker_stop(worker);
    if(master) Close(master);
    if(slave) Close(slave);
    if(copy) Close(copy);
    if(worker && aros_tty_worker_finish(worker)) {
        puts("TTY TRY WIRE FAIL: unresolved worker ownership");fflush(stdout);
        for(;;) Delay(50);
    }
    if(failed) printf("TTY TRY WIRE FAIL: line=%d error=%ld\n",failed,(long)IoErr());
    else puts("TTY TRY WIRE PASS: cycles=10, unsupported NIL; no public fd flags");
    return failed?20:0;
}
