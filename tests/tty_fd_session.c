/* Standard interfaces only. Detects mode/byte/EOF regressions across libc. */
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef TEST_LIBC_NONBLOCK
#include <fcntl.h>
#include <stdio.h>
#endif

#define REQUIRE(e) do { if(!(e)) return 20; } while(0)
#define SAY(s) REQUIRE(write(1,s,sizeof(s)-1)==(ssize_t)(sizeof(s)-1))

int main(void)
{
    struct termios saved,raw;
    struct winsize size={24,80,640,408},before=size;
#ifdef TEST_GEOMETRY_CELLS_ONLY
    size.ws_xpixel=size.ws_ypixel=0;before=size;
#endif
    unsigned char bytes[32];
    const unsigned char binary[]={'A',0,'B',27,'[','A',0xf0,0x9f,0x98,0x80};
    size_t used=0;
    REQUIRE(tcgetattr(0,&saved)==0);
    REQUIRE(saved.c_lflag==(ICANON|ECHO) && saved.c_iflag==ICRNL);
    REQUIRE(saved.c_oflag==(OPOST|ONLCR));
    /* Characterize the unimplemented public geometry boundary, not a PASS
       for geometry support. Do not fake dimensions inside this consumer. */
#ifdef TEST_LIBC_GEOMETRY
    memset(&size,0xa5,sizeof size);
    REQUIRE(ioctl(0,TIOCGWINSZ,&size)==0);
    REQUIRE(!memcmp(&size,&before,sizeof size));
#else
    REQUIRE(ioctl(0,TIOCGWINSZ,&size)==-1 && errno==ENOTTY);
    REQUIRE(!memcmp(&size,&before,sizeof size));
#endif
    raw=saved;raw.c_lflag=0;raw.c_iflag=0;raw.c_oflag=0;
    raw.c_cc[VMIN]=1;raw.c_cc[VTIME]=0;
    REQUIRE(tcsetattr(0,TCSANOW,&raw)==0);
#ifdef TEST_LIBC_NONBLOCK
    /* Real libc descriptor sharing, not the optional read/write adapters.
       A setter-only implementation hangs at the empty read below; the parent
       timeout and private-VM watchdog must reject that run. */
    {
        int copy=dup(0),flags=fcntl(0,F_GETFL),outflags=fcntl(1,F_GETFL);
        unsigned char untouched=0xa5;
#define NB_CHECK(e) do { if(!(e)) { fprintf(stderr,"LIBC NB FAIL line=%d errno=%d\n",__LINE__,errno); return 20; } } while(0)
        NB_CHECK(copy>=3 && flags>=0 && outflags>=0);
        NB_CHECK(fcntl(copy,F_SETFL,flags|O_NONBLOCK)==0);
        NB_CHECK(fcntl(0,F_GETFL)==(flags|O_NONBLOCK));
        errno=0;
        NB_CHECK(read(0,&untouched,1)==-1 && errno==EAGAIN);
        NB_CHECK(untouched==0xa5);
        NB_CHECK(read(copy,NULL,0)==0);
        errno=0;
        NB_CHECK(fcntl(copy,F_SETFL,flags|O_NONBLOCK|O_APPEND)==-1 && errno==ENOTSUP);
        NB_CHECK(fcntl(0,F_GETFL)==(flags|O_NONBLOCK));
        NB_CHECK(fcntl(0,F_SETFL,flags)==0);
        NB_CHECK(fcntl(copy,F_GETFL)==flags);
        NB_CHECK(close(copy)==0);
        NB_CHECK(fcntl(1,F_SETFL,outflags|O_NONBLOCK)==0);
        NB_CHECK(write(1,NULL,0)==0);
        NB_CHECK(fcntl(1,F_SETFL,outflags)==0);
        NB_CHECK(fcntl(1,F_GETFL)==outflags);
#undef NB_CHECK
    }
#endif
    SAY("FD_RAW_READY\n");
    while(used<sizeof binary) {
        ssize_t n=read(0,bytes+used,sizeof binary-used);
        REQUIRE(n>0 && (size_t)n<=sizeof binary-used);used+=(size_t)n;
    }
    REQUIRE(!memcmp(bytes,binary,sizeof binary));
#ifdef TEST_LIBC_GEOMETRY
    REQUIRE(ioctl(1,TIOCGWINSZ,&size)==0);
    REQUIRE(size.ws_row==22 && size.ws_col==72);
#ifdef TEST_GEOMETRY_CELLS_ONLY
    REQUIRE(!size.ws_xpixel && !size.ws_ypixel);
#else
    REQUIRE(size.ws_xpixel==576 && size.ws_ypixel==374);
#endif
#endif
    REQUIRE(write(1,bytes,used)==(ssize_t)used);
    REQUIRE(tcsetattr(0,TCSADRAIN,&saved)==0);
    SAY("FD_COOKED_READY\n");
    REQUIRE(read(0,bytes,sizeof bytes)==4 && !memcmp(bytes,"one\n",4));
    SAY("FD_EOF_READY\n");
    REQUIRE(read(0,bytes,sizeof bytes)==0);
    SAY("FD_RESTART_READY\n");
    REQUIRE(read(0,bytes,sizeof bytes)==4 && !memcmp(bytes,"two\n",4));
    REQUIRE(tcsetattr(0,TCSAFLUSH,&raw)==0);
    SAY("FD_SESSION_PASS\n");
    return 0;
}
