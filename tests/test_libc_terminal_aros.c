/* Public libc APIs only. Run in native CON or XTerm with --xterm. */
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct termios t,before,saved;
    struct winsize size={0};
    int file=-1,copy=-1,changed=0,failed=0,err=0;
    int xterm=argc==2 && !strcmp(argv[1],"--xterm");
#define CHECK(x) do { if(!(x)) { failed=__LINE__;err=errno;goto done; } } while(0)
    memset(&t,0x55,sizeof t); before=t;
    CHECK(tcgetattr(-1,&t)==-1 && errno==EBADF);
    CHECK(tcsetattr(-1,TCSANOW,&t)==-1 && errno==EBADF);
    CHECK(ioctl(-1,TIOCGWINSZ,&size)==-1 && errno==EBADF);
    file=open("RAM:aros-tty-safety-probe.tmp",O_CREAT|O_EXCL|O_RDWR,0600);
    CHECK(file>=0);
    CHECK(tcgetattr(file,&t)==-1 && errno==ENOTTY && !memcmp(&t,&before,sizeof t));
    CHECK(tcsetattr(file,TCSANOW,&t)==-1 && errno==ENOTTY);
    CHECK(ioctl(file,TIOCGWINSZ,&size)==-1 && errno==ENOTTY);
    CHECK(tcgetattr(STDIN_FILENO,NULL)==-1 && errno==EFAULT);
    CHECK(tcsetattr(STDIN_FILENO,TCSANOW,NULL)==-1 && errno==EFAULT);
    CHECK(ioctl(STDIN_FILENO,TIOCGWINSZ,NULL)==-1 && errno==ENOTTY);
    memset(&saved,0,sizeof saved);
    CHECK(tcgetattr(STDIN_FILENO,&saved)==0);
    CHECK(tcsetattr(STDIN_FILENO,99,&saved)==-1 && errno==EINVAL);
    CHECK(tcsetattr(STDIN_FILENO,TCSADRAIN,&saved)==0);
    CHECK(tcsetattr(STDIN_FILENO,TCSAFLUSH,&saved)==0);
    copy=dup(STDIN_FILENO); CHECK(copy>=0);
    t=saved; t.c_lflag &= ~ICANON;
    CHECK(tcsetattr(copy,TCSANOW,&t)==0); changed=1;
    t.c_lflag=ICANON;
    CHECK(tcgetattr(STDIN_FILENO,&t)==0 && !(t.c_lflag&ICANON));
    CHECK(tcsetattr(STDIN_FILENO,TCSANOW,&saved)==0); changed=0;
    CHECK(tcgetattr(copy,&t)==0 && (t.c_lflag&ICANON)==(saved.c_lflag&ICANON));
    size.ws_row=1; size.ws_col=2; size.ws_xpixel=3; size.ws_ypixel=4;
    CHECK(ioctl(copy,TIOCGWINSZ,&size)==-1 && errno==ENOTTY);
    CHECK(size.ws_row==1 && size.ws_col==2 && size.ws_xpixel==3 && size.ws_ypixel==4);
done:
    if(changed && tcsetattr(STDIN_FILENO,TCSANOW,&saved)) failed=__LINE__;
    if(copy>=0 && close(copy)) failed=__LINE__;
    if(file>=0) { if(close(file) || unlink("RAM:aros-tty-safety-probe.tmp")) failed=__LINE__; }
    if(failed) printf("LIBC TTY FAIL line=%d errno=%d\n",failed,err);
    else printf("LIBC TTY PASS %s: errors, legacy actions, dup mode roundtrip, geometry=ENOTTY unchanged\n",
                xterm?"XTERM":"CON");
    return failed?20:0;
}
