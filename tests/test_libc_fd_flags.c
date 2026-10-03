#include <assert.h>
#include <errno.h>
#include <stdio.h>
static fcb state;
static fdesc descriptor={&state,0};
fdesc *__getfdesc(int fd) { return fd==3?&descriptor:NULL; }
const struct fd_hooks *__getfdhooks(int fd,APTR *data) { (void)fd;(void)data;return NULL; }
int __getfirstfd(int fd) { return fd; }
int __oflags2amode(int flags) { return flags; }
int ChangeMode(int mode,BPTR handle,int flags) { (void)mode;(void)handle;(void)flags;return DOSTRUE; }
LONG IoErr(void) { return 0; }
int __stdc_ioerr2errno(LONG error) { (void)error;return EIO; }
void __fcb_lock(fcb *f) { (void)f; }
void __fcb_unlock(fcb *f) { (void)f; }
int main(void)
{
    int modes[]={O_RDONLY,O_WRONLY,O_RDWR};unsigned mode,bits;
    for(mode=0;mode<3;++mode) for(bits=0;bits<8;++bits) {
        int status=((bits&1)?O_APPEND:0)|((bits&2)?O_NONBLOCK:0)|((bits&4)?O_ASYNC:0);
        state.flags=modes[mode]|status|O_CREAT|O_TRUNC|O_EXCL;
        assert(fcntl(3,F_GETFL)==(modes[mode]|status));
    }
    assert(fcntl(-1,F_GETFL)==-1 && errno==EBADF);
    puts("LIBC FD FLAGS PASS: actual fcntl.c, 24 mode/status combinations, creation flags excluded, EBADF");
    return 0;
}
