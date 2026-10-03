/* Real GUI-side PTY adapter, with a separately loaded standard-libc child. */
#include "../src/aros_pty_frontend.h"
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static ArosSession session;
static int expect(const char *want,size_t length)
{
    unsigned tick;size_t got=0;unsigned char bytes[128];
    for(tick=0;tick<1000 && got<length;++tick) {
        aros_pty_poll(&session);
        got+=aros_pty_read(&session,bytes+got,length-got);
        if(got<length) Delay(1);
    }
    return got==length && !memcmp(bytes,want,length);
}
#define CHECK(e) do { if(!(e)) { printf("PTY FRONTEND FAIL line=%d\n",__LINE__);fflush(stdout);aros_pty_stop(&session);while(aros_pty_active(&session)){aros_pty_poll(&session);Delay(1);}aros_pty_dispose(&session);return 20;} } while(0)
#define EXPECT(s) CHECK(expect(s,sizeof(s)-1))
int main(int argc,char **argv)
{
    unsigned cycle,tick;
    CHECK(argc==3);
    for(cycle=0;cycle<5;++cycle) {
        session.columns=80;session.rows=24;
        CHECK(!aros_pty_start(&session,argv[1]));
        EXPECT("FD_RAW_READY\n");
        session.columns=72;session.rows=22;aros_pty_poll(&session);
        CHECK(aros_pty_write(&session,(const unsigned char *)"A\0B\033[A\360\237\230\200",10)==10);
        EXPECT("A\0B\033[A\360\237\230\200FD_COOKED_READY\r\n");
        CHECK(aros_pty_write(&session,(const unsigned char *)"one\r",4)==4);
        EXPECT("one\r\nFD_EOF_READY\r\n");
        CHECK(aros_pty_write(&session,(const unsigned char *)"\004",1)==1);
        EXPECT("FD_RESTART_READY\r\n");
        CHECK(aros_pty_write(&session,(const unsigned char *)"two\n",4)==4);
        EXPECT("two\r\nFD_SESSION_PASS\n");
        for(tick=0;tick<500 && aros_pty_active(&session);++tick){aros_pty_poll(&session);Delay(1);}
        CHECK(!aros_pty_active(&session) && session.exited && !session.result);
        CHECK(!aros_pty_dispose(&session));
        printf("PTY FRONTEND CYCLE PASS: %u\n",cycle+1);fflush(stdout);
    }
    CHECK(!aros_pty_start(&session,argv[2]));
    { size_t total=0;unsigned char bytes[1024];
      for(tick=0;tick<1500 && total<16387;++tick) {
          size_t n,i,limit=16387-total;if(limit>sizeof bytes) limit=sizeof bytes;
          aros_pty_poll(&session);n=aros_pty_read(&session,bytes,limit);
          for(i=0;i<n;++i) CHECK(bytes[i]==(unsigned char)((total+i)*37u+11u));
          total+=n;if(!n) Delay(1);
      }
      CHECK(total==16387); }
    EXPECT("BULK_READY");
    for(tick=0;tick<5;++tick){aros_pty_poll(&session);Delay(1);}
    aros_pty_stop(&session);
    for(tick=0;tick<500 && aros_pty_active(&session);++tick){aros_pty_poll(&session);Delay(1);}
    CHECK(!aros_pty_active(&session) && !session.result);
    CHECK(!aros_pty_dispose(&session));
    puts("PTY FRONTEND BULK PASS: 16387 exact bytes, timed-read hangup and reap");
    puts("PTY FRONTEND PASS: 5 start/resize/binary/echo/EOF/restart/reap cycles, no wrappers");
    return 0;
}
