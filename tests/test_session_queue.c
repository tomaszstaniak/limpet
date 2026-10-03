#include "../src/session_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Catches overwritten unread data, wrong wrap offset and lost short writes. */
int main(void)
{
    XSessionQueue q={0};
    unsigned char in[XSESSION_CAPACITY+17],out[XSESSION_CAPACITY+17];
    size_t i;
    for(i=0;i<sizeof in;++i) in[i]=(unsigned char)(i*37);
    assert(session_queue_read(&q,out,1)==0);
    /* Timeout must not mask queued input; EOF releases an empty waiter. */
    assert(session_queue_wait(&q,0,0)==-1);
    assert(session_queue_wait(&q,0,1)==0);
    assert(session_queue_wait(&q,1,0)==0);
    assert(session_queue_write(&q,in,sizeof in)==XSESSION_CAPACITY);
    assert(session_queue_wait(&q,0,0)==1);
    assert(session_queue_wait(&q,0,1)==1);
    assert(session_queue_wait(&q,1,1)==1);
    assert(session_queue_write(&q,in,1)==0);
    assert(session_queue_read(&q,out,31)==31);
    assert(!memcmp(out,in,31));
    assert(session_queue_write(&q,in,17)==17);
    assert(session_queue_read(&q,out,sizeof out)==XSESSION_CAPACITY-14);
    assert(!memcmp(out,in+31,XSESSION_CAPACITY-31));
    assert(!memcmp(out+XSESSION_CAPACITY-31,in,17));
    assert(!q.length);
    assert(session_queue_write(&q,NULL,0)==0);
    puts("session queue tests passed (bounds, short writes, wraparound)");
    return 0;
}
