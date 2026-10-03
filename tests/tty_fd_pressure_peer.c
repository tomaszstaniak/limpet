/* Standard libc only: no private headers, wrappers or owner-side shortcuts. */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>

#define REQUIRE(e) do { if(!(e)) return __LINE__; } while(0)
int main(void)
{
    unsigned char data[10003],ack;
    size_t offset;unsigned retries=0,i;ssize_t n;
    int flags=fcntl(1,F_GETFL),copy=dup(1);
    REQUIRE(flags>=0 && copy>=3);
    REQUIRE(fcntl(copy,F_SETFL,flags|O_NONBLOCK)==0);
    REQUIRE(fcntl(1,F_GETFL)==(flags|O_NONBLOCK));
    for(i=0;i<sizeof data;++i) data[i]=(unsigned char)(i*37u+11u);
    n=write(1,data,sizeof data);REQUIRE(n==7);offset=7;
    errno=0;
    REQUIRE(write(copy,data+offset,sizeof data-offset)==-1 && errno==EAGAIN);
    do {
        REQUIRE(read(0,&ack,1)==1 && ack=='G');++retries;
        do {
            n=write(copy,data+offset,sizeof data-offset);
            if(n>0) { REQUIRE((size_t)n<=sizeof data-offset);offset+=(size_t)n; }
            else { REQUIRE(n==-1 && errno==EAGAIN);break; }
        } while(offset<sizeof data);
    } while(offset<sizeof data && retries<32);
    REQUIRE(offset==sizeof data && retries>1 && retries<32);
    REQUIRE(fcntl(1,F_SETFL,flags)==0);
    REQUIRE(fcntl(copy,F_GETFL)==flags);
    REQUIRE(close(copy)==0);
    return 0;
}
