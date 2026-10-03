/* Opt-in only: --wrap=read --wrap=write alongside the attribute bridge.
   Requires safety-fixed libc AND F_GETFL access-mode correction.
   No concurrent descriptor close/replacement; F_SETFL and readiness remain
   separate integration gaps. Existing O_NONBLOCK flags select private TRY I/O. */
#include "aros_tty_fd_bridge.h"
#include "aros_tty_client.h"
#include <proto/dos.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

ssize_t __real_read(int,void *,size_t);
ssize_t __real_write(int,const void *,size_t);

static int validate(int fd,const void *buffer,size_t count,int writing,int *nonblocking)
{
    int flags=fcntl(fd,F_GETFL);
    if(flags<0) return -1;
    if((writing && (flags&O_ACCMODE)==O_RDONLY) ||
       (!writing && (flags&O_ACCMODE)==O_WRONLY)) { errno=EBADF;return -1; }
    if(count && !buffer) { errno=EFAULT;return -1; }
    *nonblocking=(flags&O_NONBLOCK)!=0;
    return 0;
}
static ssize_t result(LONG count,int nonblocking)
{
    if(count>=0) return count;
    /* Cancellation is an interrupted syscall, not the generic DOS mapper's
       historical translation. Malformed transport replies are I/O errors. */
    LONG error=IoErr();
    if(error==ERROR_BREAK) errno=EINTR;
    else if(nonblocking && error==ERROR_OBJECT_IN_USE) errno=EAGAIN;
    else if(error==ERROR_BAD_NUMBER) errno=EIO;
    else if(error==ERROR_ACTION_NOT_KNOWN) errno=ENOTSUP;
    else { errno=__stdc_ioerr2errno(error);if(!errno) errno=EIO; }
    return -1;
}
ssize_t __wrap_read(int fd,void *buffer,size_t count)
{
    BPTR handle;LONG length;int nonblocking,kind=aros_tty_fd_resolve(fd,&handle);
    if(kind<0) return -1;
    if(!kind) return __real_read(fd,buffer,count);
    if(validate(fd,buffer,count,0,&nonblocking)) return -1;
    /* One bounded DOS request; POSIX permits short successful transfers. */
    length=count>INT32_MAX?INT32_MAX:(LONG)count;
    return result(nonblocking?aros_tty_read_try(handle,buffer,length):
                  aros_tty_read_break(handle,buffer,length),nonblocking);
}
ssize_t __wrap_write(int fd,const void *buffer,size_t count)
{
    BPTR handle;LONG length;int nonblocking,kind=aros_tty_fd_resolve(fd,&handle);
    if(kind<0) return -1;
    if(!kind) return __real_write(fd,buffer,count);
    if(validate(fd,buffer,count,1,&nonblocking)) return -1;
    length=count>INT32_MAX?INT32_MAX:(LONG)count;
    return result(nonblocking?aros_tty_write_try(handle,buffer,length):
                  aros_tty_write_break(handle,buffer,length),nonblocking);
}
