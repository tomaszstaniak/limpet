/* Actual libc syscall sources; descriptor lookup and DOS are host boundaries.
   Two fd lookups reference one FCB. This is not an actual native dup test. */
#include "../src/aros_tty_client.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
int ioctl(int,int,...);
static struct MsgPort port;
static struct FileHandle handle={&port,7};
static fcb state;
static fdesc descriptor={&state,0};
static SIPTR error;
static LONG amount,last_length,transfer_error;
static int legacy,malformed,locked,try_calls,blocking_calls,dos_calls,probe_calls;
static int interactive,answer_mode;
static SIPTR probe_error;
static uint32_t capabilities;
static XtyControl geometry_reply;
static int geometry_calls,geometry_answer;
static SIPTR geometry_error;
static XtyControl settings;
static unsigned attr_operation,attr_calls,attr_blocking;
static LONG attr_error;
static void reset(void)
{
    state.handle=(BPTR)(intptr_t)&handle;state.flags=O_RDWR;state.privflags=0;
    error=transfer_error=0;amount=0;last_length=-1;legacy=malformed=locked=0;
    try_calls=blocking_calls=dos_calls=probe_calls=0;
    interactive=1;answer_mode=0;probe_error=0;
    capabilities=XTY_CAP_TRY_IO|XTY_CAP_CANCEL;errno=0;
    memset(&geometry_reply,0,sizeof geometry_reply);
    geometry_reply.version=XTY_CONTROL_VERSION;geometry_reply.operation=XTY_GET_GEOMETRY;
    geometry_reply.reserved[0]=XTY_PACKET_MAGIC;
    geometry_reply.values[0]=24;geometry_reply.values[1]=80;
    geometry_reply.values[2]=640;geometry_reply.values[3]=408;
    geometry_calls=geometry_answer=0;geometry_error=0;
    memset(&settings,0,sizeof settings);
    settings.version=XTY_CONTROL_VERSION;settings.operation=XTY_GET_CONFIG;
    settings.reserved[0]=XTY_PACKET_MAGIC;
    settings.values[0]=XTY_ICRNL;settings.values[1]=XTY_OPOST|XTY_ONLCR;
    settings.values[2]=XTY_CANON|XTY_ECHO;settings.values[3]=4;
    attr_operation=attr_calls=attr_blocking=0;attr_error=0;
}
fdesc *__getfdesc(int fd) { return fd==3 || fd==4?&descriptor:NULL; }
const struct fd_hooks *__getfdhooks(int fd,APTR *data) { (void)fd;(void)data;return NULL; }
int __getfirstfd(int fd) { return fd; }
LONG __oflags2amode(int flags) { return flags; }
LONG ChangeMode(int mode,BPTR h,int flags)
{ (void)mode;(void)h;(void)flags;++dos_calls;error=ERROR_ACTION_NOT_KNOWN;return DOSFALSE; }
SIPTR IoErr(void) { return error; }
int __stdc_ioerr2errno(LONG e) { (void)e;return EIO; }
void __fcb_lock(fcb *f) { assert(f==&state && !locked);locked=1; }
void __fcb_unlock(fcb *f) { assert(f==&state && locked);locked=0; }
LONG IsInteractive(BPTR h)
{ assert(h==state.handle && !(state.privflags&_FCB_ISDIR));return interactive?DOSTRUE:DOSFALSE; }
int SetMode(BPTR h,int mode)
{ assert(h==state.handle && (mode==0 || mode==1));++dos_calls;return DOSTRUE; }
LONG aros_tty_control(BPTR h,XtyControl *c)
{
    assert(h==state.handle);error=attr_error;
    if(attr_error) return DOSFALSE;
    if(c->operation==XTY_CAPS) { c->values[0]=capabilities;c->reserved[0]=XTY_PACKET_MAGIC;return DOSTRUE; }
    attr_operation=c->operation;++attr_calls;
    if(c->operation==XTY_GET_CONFIG) *c=settings;
    else { settings=*c;settings.operation=XTY_GET_CONFIG;settings.reserved[0]=XTY_PACKET_MAGIC; }
    return DOSTRUE;
}
LONG aros_tty_control_break(BPTR h,XtyControl *c)
{ ++attr_blocking;return aros_tty_control(h,c); }
SIPTR DoPkt(struct MsgPort *p,LONG action,SIPTR id,SIPTR payload,SIPTR n,SIPTR magic,SIPTR reserved)
{
    XtyControl *c=(XtyControl *)payload;
    assert(p==&port && action==XTY_ACTION_CONTROL && id==7 && n==64);
    assert(magic==XTY_PACKET_MAGIC && !reserved);
    if(c->operation==XTY_GET_GEOMETRY) {
        ++geometry_calls;*c=geometry_reply;error=geometry_error;
        return geometry_answer==1?DOSFALSE:geometry_answer==2?(SIPTR)UINT64_C(0xffffffff):DOSTRUE;
    }
    assert(c->operation==XTY_CAPS);
    ++probe_calls;
    if(legacy) { error=ERROR_ACTION_NOT_KNOWN;return DOSFALSE; }
    c->reserved[0]=malformed?0:XTY_PACKET_MAGIC;c->values[0]=capabilities;
    error=probe_error;
    if(answer_mode==1) return DOSFALSE;
    if(answer_mode==2) return (SIPTR)UINT64_C(0xffffffff); /* Would narrow to -1. */
    return DOSTRUE;
}
static LONG transfer(BPTR h,const void *b,LONG n,int writing,int immediate)
{
    assert(h==state.handle && !locked);last_length=n;error=transfer_error;
    if(immediate) ++try_calls;else ++blocking_calls;
    if(!writing && amount>0) { assert(amount==2);memcpy((void *)b,"A\0",2); }
    return amount;
}
LONG aros_tty_read_try(BPTR h,void *b,LONG n) { return transfer(h,b,n,0,1); }
LONG aros_tty_write_try(BPTR h,const void *b,LONG n) { return transfer(h,b,n,1,1); }
LONG aros_tty_read_break(BPTR h,void *b,LONG n) { return transfer(h,b,n,0,0); }
LONG aros_tty_write_break(BPTR h,const void *b,LONG n) { return transfer(h,b,n,1,0); }
QUAD __dos64_read(fcb *f,void *b,QUAD n)
{ (void)f;(void)b;(void)n;++dos_calls;return 0; }
QUAD __dos64_write(fcb *f,const void *b,QUAD n)
{ (void)f;(void)b;++dos_calls;return n; }
static void flags_and_io(void)
{
    char b[8]="xxxxxxx";int initial;
    reset();initial=fcntl(3,F_GETFL);assert(initial==O_RDWR);
    assert(fcntl(3,F_SETFL,initial|O_NONBLOCK)==0);
    assert(fcntl(4,F_GETFL)==(O_RDWR|O_NONBLOCK) && !dos_calls && !locked);
    amount=-1;transfer_error=ERROR_OBJECT_IN_USE;
    assert(read(3,b,8)==-1 && errno==EAGAIN && !memcmp(b,"xxxxxxx",8));
    assert(write(4,b,8)==-1 && errno==EAGAIN);
    transfer_error=0;amount=2;
    assert(read(4,b,8)==2 && !memcmp(b,"A\0xxxxx",8) && try_calls==3);
    assert(write(3,b,(size_t)INT32_MAX+1)==2 && last_length==INT32_MAX && try_calls==4);
    amount=0;assert(read(3,b,8)==0 && read(4,NULL,0)==0);
    assert(fcntl(4,F_SETFL,O_RDONLY)==0); /* Access bits are ignored by F_SETFL. */
    assert(fcntl(3,F_GETFL)==O_RDWR);
    assert(read(3,b,8)==0 && blocking_calls==1);
    assert(!dos_calls && !locked);
}
static void rejection(void)
{
    const int unsupported[]={O_APPEND,O_ASYNC};unsigned i;
    for(i=0;i<2;++i) {
        reset();assert(fcntl(3,F_SETFL,O_NONBLOCK|unsupported[i])==-1 && errno==ENOTSUP);
        assert(state.flags==O_RDWR && !dos_calls && !locked);
        state.flags|=O_NONBLOCK;
        assert(fcntl(4,F_SETFL,unsupported[i])==-1 && errno==ENOTSUP);
        assert(state.flags==(O_RDWR|O_NONBLOCK) && !dos_calls && !locked);
    }
    reset();capabilities=XTY_CAP_CANCEL;
    assert(fcntl(3,F_SETFL,O_NONBLOCK)==-1 && errno==ENOTSUP && state.flags==O_RDWR);
    state.flags|=O_NONBLOCK;
    assert(read(3,NULL,0)==-1 && errno==ENOTSUP && !blocking_calls && !dos_calls);
    reset();capabilities=XTY_CAP_TRY_IO;
    assert(fcntl(3,F_SETFL,O_NONBLOCK)==-1 && errno==ENOTSUP && state.flags==O_RDWR);
    reset();malformed=1;
    assert(fcntl(3,F_SETFL,O_NONBLOCK)==-1 && errno==EIO && state.flags==O_RDWR);
    reset();state.privflags=_FCB_ISDIR;
    assert(read(3,NULL,0)==-1 && errno==EISDIR && !probe_calls);
    reset();legacy=1;
    assert(fcntl(3,F_SETFL,O_NONBLOCK)==-1 && dos_calls==1 && state.flags==O_RDWR);
    assert(read(3,NULL,0)==0 && dos_calls==2);
    assert(write(3,"ok",2)==2 && dos_calls==3);
    reset();assert(fcntl(-1,F_SETFL,O_NONBLOCK)==-1 && errno==EBADF && !probe_calls);
    assert(read(-1,NULL,0)==-1 && errno==EBADF);
}
static void probe_failures_and_validation(void)
{
    const struct { SIPTR error;int answer,expected; } cases[]={
        {INT64_C(0x100000000),0,EIO},
        {INT64_C(0x100000000)+ERROR_ACTION_NOT_KNOWN,1,EIO},
        {ERROR_BREAK,1,EINTR},{ERROR_INVALID_LOCK,1,EBADF},
        {0,1,EIO},{0,2,EIO},{ERROR_BREAK,0,EINTR}
    };
    unsigned i;char bytes[8]="xxxxxxx";
    for(i=0;i<sizeof cases/sizeof cases[0];++i) {
        reset();probe_error=cases[i].error;answer_mode=cases[i].answer;
        assert(fcntl(3,F_SETFL,O_NONBLOCK)==-1 && errno==cases[i].expected);
        assert(state.flags==O_RDWR && !dos_calls);
        assert(read(3,bytes,8)==-1 && errno==cases[i].expected);
        assert(write(4,bytes,8)==-1 && errno==cases[i].expected);
        assert(!try_calls && !blocking_calls && !dos_calls && !memcmp(bytes,"xxxxxxx",8));
    }
    reset();interactive=0;
    assert(read(3,bytes,8)==0 && write(3,bytes,8)==8 && !probe_calls && dos_calls==2);
    reset();state.flags=O_RDONLY|O_NONBLOCK;
    assert(write(3,bytes,8)==-1 && errno==EBADF && !try_calls);
    state.flags=O_WRONLY|O_NONBLOCK;
    assert(read(3,bytes,8)==-1 && errno==EBADF && !try_calls);
    state.flags=O_RDWR|O_NONBLOCK;
    assert(read(3,NULL,1)==-1 && errno==EFAULT && !try_calls);
    assert(write(3,NULL,1)==-1 && errno==EFAULT && !try_calls);
    transfer_error=ERROR_BREAK;amount=-1;
    assert(read(3,bytes,8)==-1 && errno==EINTR);
    transfer_error=ERROR_BAD_NUMBER;
    assert(write(3,bytes,8)==-1 && errno==EIO);
}
static void geometry(void)
{
    struct winsize out,before;unsigned i;
    reset();capabilities|=XTY_CAP_GET_GEOMETRY;
    memset(&out,0xa5,sizeof out);
    assert(ioctl(3,TIOCGWINSZ,&out)==0);
    assert(out.ws_row==24 && out.ws_col==80 && out.ws_xpixel==640 && out.ws_ypixel==408);
    geometry_reply.values[0]=22;geometry_reply.values[1]=72;
    assert(ioctl(4,TIOCGWINSZ,&out)==0 && out.ws_row==22 && out.ws_col==72);
    assert(ioctl(3,TIOCGWINSZ,NULL)==-1 && errno==EFAULT && geometry_calls==2);
    for(i=0;i<4;++i) {
        reset();capabilities|=XTY_CAP_GET_GEOMETRY;before=out;
        geometry_reply.values[i]=65536;
        assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==EOVERFLOW && !memcmp(&out,&before,sizeof out));
    }
    for(i=0;i<10;++i) {
        reset();capabilities|=XTY_CAP_GET_GEOMETRY;before=out;
        if(i==0) geometry_reply.values[0]=0;
        if(i==1) geometry_reply.values[1]=0;
        if(i==2) geometry_reply.values[11]=1;
        if(i==3) geometry_reply.reserved[0]=0;
        if(i==4) geometry_reply.operation=XTY_GET_CONFIG;
        if(i==5) geometry_answer=2;
        if(i==6) geometry_reply.version=2;
        if(i==7) geometry_reply.reserved[1]=1;
        if(i==8) geometry_answer=1;
        if(i==9) geometry_error=INT64_C(0x100000000);
        assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==EIO && !memcmp(&out,&before,sizeof out));
    }
    reset();capabilities|=XTY_CAP_GET_GEOMETRY;before=out;
    geometry_error=ERROR_BREAK;geometry_answer=1;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==EINTR && !memcmp(&out,&before,sizeof out));
    geometry_error=ERROR_INVALID_LOCK;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==EBADF && !memcmp(&out,&before,sizeof out));
    geometry_error=ERROR_ACTION_NOT_KNOWN;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==ENOTSUP && !memcmp(&out,&before,sizeof out));
    reset();capabilities|=XTY_CAP_GET_GEOMETRY;
    geometry_reply.values[0]=65535;geometry_reply.values[1]=65535;
    geometry_reply.values[2]=0;geometry_reply.values[3]=0;
    assert(ioctl(3,TIOCGWINSZ,&out)==0 && out.ws_row==65535 && out.ws_col==65535 && !out.ws_xpixel && !out.ws_ypixel);
    reset();before=out;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==ENOTSUP && !geometry_calls && !memcmp(&out,&before,sizeof out));
    assert(ioctl(3,TIOCGWINSZ)==-1 && errno==ENOTSUP && !geometry_calls);
    reset();legacy=1;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==ENOTTY && !geometry_calls);
    assert(ioctl(3,TIOCGWINSZ)==-1 && errno==ENOTTY && !geometry_calls);
    reset();state.privflags=_FCB_ISDIR;
    assert(ioctl(3,TIOCGWINSZ,&out)==-1 && errno==ENOTTY && !probe_calls);
    assert(ioctl(-1,TIOCGWINSZ,&out)==-1 && errno==EBADF && !probe_calls);
    assert(ioctl(3,12345)==-1 && errno==ENOTTY && !probe_calls);
    puts("LIBC GEOMETRY HOST PASS: dimensions, fresh query, validation, unchanged errors, legacy fallback");
}
static void attributes(void)
{
    struct termios t,before;unsigned probes;
    const int actions[]={TCSANOW,TCSADRAIN,TCSAFLUSH};
    const unsigned operations[]={XTY_SET_CONFIG,XTY_DRAIN_CONFIG,XTY_DRAIN_CONFIG_DISCARD_INPUT};
    unsigned i;
    reset();capabilities=65535;
    memset(&t,0xa5,sizeof t);
    assert(tcgetattr(3,&t)==0 && t.c_lflag==(ICANON|ECHO));
    assert(t.c_iflag==ICRNL && t.c_oflag==(OPOST|ONLCR) && t.c_cc[VEOF]==4);
    assert(cfgetispeed(&t)==B38400 && cfgetospeed(&t)==B38400);
    t.c_lflag=0;t.c_iflag=0;t.c_oflag=0;before=t;
    for(i=0;i<3;++i) {
        assert(tcsetattr(4,actions[i],&t)==0 && attr_operation==operations[i]);
        assert(!memcmp(&t,&before,sizeof t) && !dos_calls);
    }
    assert(attr_blocking==2);
    memset(&t,0xa5,sizeof t);assert(tcgetattr(3,&t)==0 && !t.c_lflag && !t.c_iflag && !t.c_oflag);
    before=t;capabilities&=~XTY_CAP_CANCEL;probes=attr_calls;
    assert(tcsetattr(3,TCSADRAIN,&t)==-1 && errno==ENOTSUP && attr_calls==probes);
    t.c_iflag=IXON;
    assert(tcsetattr(3,TCSANOW,&t)==-1 && errno==ENOTSUP && attr_calls==probes);
    t=before;settings.values[11]=1;
    assert(tcgetattr(3,&t)==-1 && errno==EIO && !memcmp(&t,&before,sizeof t));
    attr_error=ERROR_BREAK;
    assert(tcgetattr(3,&t)==-1 && errno==EINTR && !memcmp(&t,&before,sizeof t));
    assert(tcsetattr(3,TCSANOW,&t)==-1 && errno==EINTR);
    reset();memset(&t,0,sizeof t);
    assert(tcgetattr(-1,&t)==-1 && errno==EBADF);
    assert(tcgetattr(3,NULL)==-1 && errno==EFAULT);
    assert(tcsetattr(3,99,&t)==-1 && errno==EINVAL);
    assert(tcsetattr(3,TCSANOW,NULL)==-1 && errno==EFAULT && !probe_calls && !attr_calls);
    state.privflags=_FCB_ISDIR;
    assert(tcgetattr(3,&t)==-1 && errno==ENOTTY && !probe_calls);
    assert(tcsetattr(3,TCSANOW,&t)==-1 && errno==ENOTTY && !probe_calls);
    reset();legacy=1;memset(&t,0,sizeof t);
    assert(tcgetattr(3,&t)==0 && t.c_lflag==ICANON && !attr_calls);
    t.c_lflag=0;
    assert(tcsetattr(3,TCSAFLUSH,&t)==0 && dos_calls==1 && (state.privflags&_FCB_CONSOLERAW));
    assert(tcgetattr(4,&t)==0 && !t.c_lflag && !attr_calls);
    puts("LIBC ATTRIBUTES HOST PASS: actual syscalls/codec, NOW/DRAIN/FLUSH routing, errors, legacy fallback");
}
int main(void)
{
    attributes();
    geometry();
    flags_and_io();rejection();probe_failures_and_validation();
    puts("LIBC TTY EXPERIMENT HOST PASS: shared FCB flags, TRY/blocking routing, bounded binary I/O, atomic rejection, legacy fallback");
    return 0;
}
