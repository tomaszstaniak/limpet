/* Compile the actual fd adapter; substitute only native boundaries.
   This does not exercise DOS packets, fd resolution or native cancellation. */
#include "../src/aros_tty_fd_bridge.h"
#include "../src/aros_tty_client.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

ssize_t __wrap_read(int,void *,size_t);
ssize_t __wrap_write(int,const void *,size_t);
static int kind,flags,flag_calls,io_calls,legacy_calls,mapper_calls;
static LONG transferred,dos_error,requested;
static int direction;
static int try_calls;
static const void *expected_buffer;
static size_t expected_count;
static void reset(void)
{
    kind=1;flags=O_RDWR;flag_calls=io_calls=legacy_calls=mapper_calls=0;
    transferred=0;dos_error=0;requested=-1;errno=0;
    try_calls=0;
}
int aros_tty_fd_resolve(int fd,BPTR *out)
{
    assert(fd==7);
    if(kind<0) errno=EBADF;
    *out=123;
    return kind;
}
/* Renamed at compile time; never interpose on the host runtime. */
int fd_test_fcntl(int fd,int command,...)
{
    assert(fd==7 && command==F_GETFL);++flag_calls;
    if(flags<0) errno=EBADF;
    return flags;
}
LONG IoErr(void) { return dos_error; }
int __stdc_ioerr2errno(LONG error)
{
    assert(error==999 || error==0 || error==ERROR_OBJECT_IN_USE);++mapper_calls;
    if(error==ERROR_OBJECT_IN_USE) return EBUSY;
    return error?ENOSPC:0;
}
static LONG transfer(BPTR h,const void *buffer,LONG count,int writing)
{
    assert(h==123 && buffer==expected_buffer && writing==direction);
    ++io_calls;requested=count;
    if(!writing && transferred>0) {
        assert(transferred==3);memcpy((void *)buffer,"A\0B",3);
    }
    return transferred;
}
LONG aros_tty_read_break(BPTR h,void *b,LONG n) { return transfer(h,b,n,0); }
LONG aros_tty_write_break(BPTR h,const void *b,LONG n) { return transfer(h,b,n,1); }
LONG aros_tty_read_try(BPTR h,void *b,LONG n)
{ ++try_calls;return transfer(h,b,n,0); }
LONG aros_tty_write_try(BPTR h,const void *b,LONG n)
{ ++try_calls;return transfer(h,b,n,1); }
ssize_t __real_read(int fd,void *b,size_t n)
{
    assert(fd==7 && !direction && b==expected_buffer && n==expected_count);
    ++legacy_calls;errno=EAGAIN;return -1;
}
ssize_t __real_write(int fd,const void *b,size_t n)
{
    assert(fd==7 && direction && b==expected_buffer && n==expected_count);
    ++legacy_calls;return 2;
}
static ssize_t call(void *buffer,size_t count)
{
    expected_buffer=buffer;expected_count=count;
    return direction?__wrap_write(7,buffer,count):__wrap_read(7,buffer,count);
}
static void rejected(void *buffer,size_t count,int error)
{
    assert(call(buffer,count)==-1 && errno==error);
    assert(!io_calls && !legacy_calls);
}
static void validation(void)
{
    char bytes[8]="xxxxxxx";
    reset();kind=-1;rejected(bytes,1,EBADF);assert(!flag_calls);
    reset();flags=-1;rejected(bytes,1,EBADF);
    reset();flags=direction?O_RDONLY:O_WRONLY;rejected(bytes,1,EBADF);
    reset();rejected(NULL,1,EFAULT);
    reset();assert(call(NULL,0)==0 && io_calls==1 && requested==0);
    assert(!try_calls);
    assert(!memcmp(bytes,"xxxxxxx",8));
}
static void nonblocking(void)
{
    char bytes[8]="xxxxxxx";
    reset();flags=O_RDWR|O_NONBLOCK;transferred=-1;
    dos_error=ERROR_OBJECT_IN_USE;
    assert(call(bytes,8)==-1 && errno==EAGAIN);
    assert(io_calls==1 && try_calls==1 && !mapper_calls);
    assert(!memcmp(bytes,"xxxxxxx",8));
    reset();flags=O_RDWR|O_NONBLOCK;transferred=3;
    assert(call(bytes,8)==3 && io_calls==1 && try_calls==1);
    assert(!memcmp(bytes,direction?"xxxxxxx":"A\0Bxxxx",8));
    reset();flags=O_RDWR|O_NONBLOCK;
    assert(call(NULL,0)==0 && io_calls==1 && try_calls==1 && requested==0);
    reset();flags=O_RDWR|O_NONBLOCK;
    assert(call(bytes,8)==0 && try_calls==1); /* EOF is not EAGAIN. */
    reset();flags=O_RDWR|O_NONBLOCK;transferred=3;
    assert(call(bytes,(size_t)INT32_MAX+1)==3 && requested==INT32_MAX && try_calls==1);
    reset();flags=O_RDWR|O_NONBLOCK;transferred=-1;dos_error=ERROR_ACTION_NOT_KNOWN;
    assert(call(bytes,8)==-1 && errno==ENOTSUP && try_calls==1 && io_calls==1);
    /* Clearing the observed flag must select blocking I/O on the next call.
       This does not emulate a successful native F_SETFL. */
    flags=O_RDWR;dos_error=ERROR_BREAK;
    assert(call(bytes,8)==-1 && errno==EINTR && try_calls==1 && io_calls==2);
    reset();flags=(direction?O_RDONLY:O_WRONLY)|O_NONBLOCK;
    rejected(bytes,1,EBADF);
    reset();flags=O_RDWR|O_NONBLOCK;rejected(NULL,1,EFAULT);
}
static void transfers(void)
{
    char bytes[8]="xxxxxxx";
    reset();transferred=3;
    assert(call(bytes,8)==3 && requested==8 && io_calls==1);
    assert(!memcmp(bytes,direction?"xxxxxxx":"A\0Bxxxx",8));
    reset();flags=direction?O_WRONLY:O_RDONLY;transferred=3;
    assert(call(bytes,8)==3 && requested==8 && io_calls==1);
    reset();transferred=3;
    assert(call(bytes,(size_t)INT32_MAX+1)==3 && requested==INT32_MAX);
    reset();transferred=0;
    assert(call(bytes,8)==0 && io_calls==1);
}
static void errors(void)
{
    static const struct { LONG dos;int posix; } cases[]={
        {ERROR_BREAK,EINTR},{ERROR_BAD_NUMBER,EIO},
        {ERROR_ACTION_NOT_KNOWN,ENOTSUP},{999,ENOSPC},{0,EIO},
        {ERROR_OBJECT_IN_USE,EBUSY}
    };
    char bytes[8]="xxxxxxx";size_t i;int nb;
    for(nb=0;nb<2;++nb) for(i=0;i<sizeof cases/sizeof cases[0];++i) {
        int again=nb && cases[i].dos==ERROR_OBJECT_IN_USE;
        reset();flags=O_RDWR|(nb?O_NONBLOCK:0);transferred=-1;dos_error=cases[i].dos;
        assert(call(bytes,8)==-1 && errno==(again?EAGAIN:cases[i].posix));
        assert(io_calls==1 && try_calls==nb && mapper_calls==(i>=3 && !again));
        assert(!memcmp(bytes,"xxxxxxx",8));
    }
}
static void fallback(void)
{
    char bytes[8]="xxxxxxx";
    reset();kind=0;flags=-1;
    assert(call(bytes,8)==(direction?2:-1));
    if(!direction) assert(errno==EAGAIN);
    assert(legacy_calls==1 && !flag_calls && !io_calls);
}
int main(void)
{
    int d;
    for(d=0;d<2;++d) {
        direction=d;validation();
        direction=d;transfers();
        direction=d;nonblocking();
        direction=d;errors();
        direction=d;fallback();
    }
    puts("TTY FD IO HOST PASS: validation, short/bounded I/O, TRY selection, EAGAIN/EOF, errno, fallback (both directions)");
    return 0;
}
