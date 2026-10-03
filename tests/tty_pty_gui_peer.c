#include <termios.h>
#include <unistd.h>
#include <errno.h>
int main(void)
{
    struct termios t;unsigned char bytes[16387],b;size_t i,offset=0;
    if(tcgetattr(0,&t)) return 20;
    t.c_iflag=t.c_oflag=t.c_lflag=0;t.c_cc[VMIN]=0;t.c_cc[VTIME]=1;
    if(tcsetattr(0,TCSANOW,&t)) return 20;
    for(i=0;i<sizeof bytes;++i) bytes[i]=(unsigned char)(i*37u+11u);
    while(offset<sizeof bytes) {
        ssize_t n=write(1,bytes+offset,sizeof bytes-offset);
        if(n<=0) return 20;
        offset+=(size_t)n;
    }
    if(write(1,"BULK_READY",10)!=10) return 20;
    for(;;) {
        ssize_t n=read(0,&b,1);
        if(n<0) {
            unsigned retry;
            if(errno!=EINTR && errno!=EBADF) return 20;
            /* Rust/std and POSIX write_all retry EINTR: a permanently dead
               endpoint must reject fresh I/O with a non-retryable error. */
            for(retry=0;retry<3;++retry) {
                if(write(1,"done",4)<0 && errno!=EINTR) return 0;
            }
            return 20;
        }
    }
}
