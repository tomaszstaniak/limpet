/* Independent consumer: standard fd API, no terminal-private headers. */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    char bytes[8]="XXXXXXX";ssize_t n;int fd,flags,current;
#ifdef XTY_WRITE_PROBE
    n=write(1,"MORE",4);
#else
    n=read(0,bytes,sizeof bytes);
#endif
    if(n!=-1 || errno!=EINTR || memcmp(bytes,"XXXXXXX",8)) return 20;
#ifdef XTY_WRITE_PROBE
    fd=1;
#else
    fd=0;
#endif
    /* Characterize a missing integration, not nonblocking I/O support:
       libc F_SETFL sends ChangeMode, unsupported by the private handler.
       Rejection must leave the descriptor flags unchanged. */
    flags=fcntl(fd,F_GETFL);if(flags<0) return 27;
#ifdef TEST_LIBC_NONBLOCK
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)!=0) return 28;
    current=fcntl(fd,F_GETFL);
    if(current!=(flags|O_NONBLOCK)) return 29;
    if(fcntl(fd,F_SETFL,flags)!=0 || fcntl(fd,F_GETFL)!=flags) return 30;
#else
    errno=0;
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)!=-1 || !errno) return 28;
    current=fcntl(fd,F_GETFL);
    if(current!=flags) return 29;
#endif
#ifdef XTY_WRITE_PROBE
    if(write(0,"X",1)!=-1 || errno!=EBADF) return 21;
    if(write(1,NULL,0)!=0) return 22;
    if(write(1,NULL,1)!=-1 || errno!=EFAULT) return 23;
    if(write(-1,"X",1)!=-1 || errno!=EBADF) return 24;
    fd=open("NIL:",O_WRONLY);
    if(fd<0 || write(fd,"file",4)!=4) return 25;
#else
    if(read(1,bytes,1)!=-1 || errno!=EBADF) return 21;
    if(read(0,NULL,0)!=0) return 22;
    if(read(0,NULL,1)!=-1 || errno!=EFAULT) return 23;
    if(read(-1,bytes,1)!=-1 || errno!=EBADF) return 24;
    fd=open("NIL:",O_RDONLY);
    if(fd<0 || read(fd,bytes,sizeof bytes)!=0) return 25;
#endif
    return close(fd)?26:0;
}
