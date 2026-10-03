/* Real fd I/O adapter + real owner core. Only fd discovery/flags and the DOS
   transport boundary are substituted. Not native F_SETFL/dup acceptance. */
#include "../src/aros_tty_fd_bridge.h"
#include "../src/aros_tty_client.h"
#include "../src/tty_service.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

ssize_t __wrap_read(int,void *,size_t);
ssize_t __wrap_write(int,const void *,size_t);
static TtyService service;
static LONG error;

int aros_tty_fd_resolve(int fd,BPTR *out)
{ assert(fd==7 || fd==8);*out=fd==7?2:1;return 1; }
int fd_test_fcntl(int fd,int command,...)
{ assert((fd==7 || fd==8) && command==F_GETFL);return O_RDWR|O_NONBLOCK; }
LONG IoErr(void) { return error; }
int __stdc_ioerr2errno(LONG e) { (void)e;assert(!"unexpected DOS error");return EIO; }
ssize_t __real_read(int fd,void *b,size_t n)
{ (void)fd;(void)b;(void)n;assert(!"legacy read selected");return -1; }
ssize_t __real_write(int fd,const void *b,size_t n)
{ (void)fd;(void)b;(void)n;assert(!"legacy write selected");return -1; }
LONG aros_tty_read_break(BPTR h,void *b,LONG n)
{ (void)h;(void)b;(void)n;assert(!"blocking read selected");return -1; }
LONG aros_tty_write_break(BPTR h,const void *b,LONG n)
{ (void)h;(void)b;(void)n;assert(!"blocking write selected");return -1; }
static LONG transfer(BPTR h,TtyOperation op,void *b,LONG n)
{
    TtyResult r;assert(n>=0);
    r=tty_service_try_io(&service,(TtyHandle)h,op,b,(size_t)n);
    if(r.status==TTY_OK || r.status==TTY_EOF) { error=0;return (LONG)r.count; }
    assert(r.status==TTY_AGAIN);error=ERROR_OBJECT_IN_USE;return -1;
}
LONG aros_tty_read_try(BPTR h,void *b,LONG n)
{ return transfer(h,TTY_REQUEST_READ,b,n); }
LONG aros_tty_write_try(BPTR h,const void *b,LONG n)
{ return transfer(h,TTY_REQUEST_WRITE,(void *)b,n); }
static void again(ssize_t n) { assert(n==-1 && errno==EAGAIN); }
static void clean(void)
{
    TtyRequest *pending=(TtyRequest *)1;uint64_t token=1;
    assert(!tty_service_take(&service));
    assert(!tty_service_read_timer(&service,&pending,&token) && !pending && !token);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_retired(&service));
}
static void raw_and_pressure(void)
{
    char bytes[8]="xxxxxxx",fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];
    TtyConfig c;tty_service_init(&service);c=service.pair.config;
    c.vmin=3;c.vtime=2;assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    again(__wrap_read(7,bytes,sizeof bytes));assert(!memcmp(bytes,"xxxxxxx",8));
    assert(__wrap_write(8,"A\0",2)==2);
    assert(__wrap_read(7,bytes,sizeof bytes)==2 && !memcmp(bytes,"A\0xxxxx",8));
    assert(__wrap_read(7,NULL,0)==0 && __wrap_write(7,NULL,0)==0);
    memset(fill,'q',sizeof fill);
    assert(__wrap_write(7,fill,sizeof fill-1)==(ssize_t)sizeof fill-1);
    assert(__wrap_write(7,"yz",2)==1);
    again(__wrap_write(7,"BAD",3));
    assert(__wrap_read(8,out,sizeof out)==(ssize_t)sizeof out);
    assert(!memcmp(out,fill,sizeof out-1) && out[sizeof out-1]=='y');
    again(__wrap_read(8,out,sizeof out));
    assert(__wrap_write(7,"z",1)==1);
    assert(__wrap_read(8,out,sizeof out)==1 && out[0]=='z');
    again(__wrap_read(8,out,sizeof out));clean();
}
static void canonical_restart(void)
{
    char bytes[8]="xxxxxxx";TtyConfig c;
    tty_service_init(&service);c=service.pair.config;c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    assert(__wrap_write(8,"a",1)==1);again(__wrap_read(7,bytes,sizeof bytes));
    assert(!memcmp(bytes,"xxxxxxx",8));
    assert(__wrap_write(8,"\n",1)==1);
    assert(__wrap_read(7,bytes,sizeof bytes)==2 && !memcmp(bytes,"a\n",2));
    assert(__wrap_write(8,"\4",1)==1);
    assert(__wrap_read(7,bytes,sizeof bytes)==0);
    again(__wrap_read(7,bytes,sizeof bytes));
    assert(__wrap_write(8,"b\n",2)==2);
    assert(__wrap_read(7,bytes,sizeof bytes)==2 && !memcmp(bytes,"b\n",2));
    clean();
}
static void ordered_read_and_drain(void)
{
    TtyRequest r;TtyConfig c;char old[8]="xxxxxxx",bytes[8]="xxxxxxx";
    tty_service_init(&service);c=service.pair.config;c.vmin=3;c.vtime=2;
    assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    memset(&r,0,sizeof r);r.handle=2;r.operation=TTY_REQUEST_READ;r.buffer=old;r.length=8;
    assert(tty_service_submit(&service,&r)==TTY_OK);
    assert(__wrap_write(8,"ab",2)==2);
    again(__wrap_read(7,bytes,sizeof bytes));assert(!memcmp(bytes,"xxxxxxx",8));
    assert(__wrap_write(8,"cd",2)==2);
    assert(tty_service_take(&service)==&r && r.result.status==TTY_OK && r.result.count==4);
    assert(!memcmp(old,"abcdxxx",8));again(__wrap_read(7,bytes,sizeof bytes));
    assert(__wrap_write(7,"x",1)==1);
    memset(&r,0,sizeof r);r.handle=2;r.operation=TTY_REQUEST_DRAIN;
    assert(tty_service_submit(&service,&r)==TTY_OK);
    again(__wrap_write(7,"y",1));
    assert(__wrap_read(8,bytes,sizeof bytes)==1 && bytes[0]=='x');
    assert(tty_service_take(&service)==&r && r.result.status==TTY_OK && !r.result.count);
    assert(__wrap_write(7,"y",1)==1);
    assert(__wrap_read(8,bytes,sizeof bytes)==1 && bytes[0]=='y');clean();
}
int main(void)
{
    unsigned i;
    for(i=0;i<100;++i) { raw_and_pressure();canonical_restart();ordered_read_and_drain(); }
    puts("TTY FD TRY FLOW PASS: 100 cycles x 3 flows; binary, pressure/retry, canonical EOF/restart, ordered read/drain, no pending requests/timers");
    return 0;
}
