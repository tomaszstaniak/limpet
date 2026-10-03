#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
int main(void)
{
    const int modes[]={O_RDONLY,O_WRONLY,O_RDWR};unsigned i;
    for(i=0;i<3;++i) {
        int fd=open("NIL:",modes[i]),copy,flags;
        if(fd<0) return 20;
        flags=fcntl(fd,F_GETFL);copy=dup(fd);
        if(flags<0 || (flags&O_ACCMODE)!=modes[i] || copy<0 ||
           (fcntl(copy,F_GETFL)&O_ACCMODE)!=modes[i]) {
            printf("LIBC FD FLAGS FAIL: mode=%d flags=%d\n",modes[i],flags);return 20;
        }
        if(close(copy) || close(fd)) return 20;
    }
    puts("LIBC FD FLAGS PASS: access modes=3, dup=3, cleanup");return 0;
}
