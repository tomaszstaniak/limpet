/* Standard termios/fd APIs only; deliberately no terminal-private headers. */
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct termios saved,t,before;int copy=-1,file=-1,failed=0;
#define CHECK(x) do { if(!(x)) { failed=__LINE__;goto done; } } while(0)
    memset(&saved,0xa5,sizeof saved);
    CHECK(tcgetattr(0,&saved)==0);
    CHECK(saved.c_iflag==ICRNL && saved.c_oflag==(OPOST|ONLCR));
    CHECK(saved.c_lflag==(ICANON|ECHO) && saved.c_cc[VEOF]==4 && saved.c_cc[VMIN]==1);
    copy=dup(0);CHECK(copy>=0);
    t=saved;t.c_lflag=0;t.c_cc[VMIN]=3;t.c_cc[VTIME]=2;
    CHECK(tcsetattr(copy,TCSANOW,&t)==0);
    memset(&before,0xa5,sizeof before);CHECK(tcgetattr(0,&before)==0);
    CHECK(before.c_lflag==0 && before.c_cc[VMIN]==3 && before.c_cc[VTIME]==2);
    CHECK(tcsetattr(0,TCSADRAIN,&saved)==0);
    CHECK(tcgetattr(copy,&t)==0 && t.c_lflag==saved.c_lflag);
    CHECK(tcsetattr(0,TCSAFLUSH,&saved)==0);
    CHECK(tcsetattr(0,999,&saved)==-1 && errno==EINVAL);
    CHECK(tcgetattr(0,NULL)==-1 && errno==EFAULT);
    memset(&t,0x55,sizeof t);before=t;
    CHECK(tcgetattr(-1,&t)==-1 && errno==EBADF && !memcmp(&t,&before,sizeof t));
    file=open("NIL:",O_RDONLY);CHECK(file>=0);
    CHECK(tcgetattr(file,&t)==-1 && errno==ENOTTY && !memcmp(&t,&before,sizeof t));
    CHECK(close(file)==0);file=-1;
    file=open("RAM:",O_RDONLY);CHECK(file>=0);
    CHECK(tcgetattr(file,&t)==-1 && errno==ENOTTY && !memcmp(&t,&before,sizeof t));
    CHECK(tcsetattr(file,TCSANOW,&saved)==-1 && errno==ENOTTY);
    CHECK(close(file)==0);file=-1;
    CHECK(close(copy)==0);copy=-1;
    CHECK(tcgetattr(0,&t)==0 && t.c_lflag==saved.c_lflag);
done:
    if(copy>=0) close(copy);
    if(file>=0) close(file);
    if(failed) printf("TTY FD CONSUMER FAIL line=%d errno=%d\n",failed,errno);
    else puts("TTY FD CONSUMER PASS: standard APIs, inherited stdin, dup shared attributes, actions=3, errors, surviving original descriptor");
    return failed?20:0;
}
