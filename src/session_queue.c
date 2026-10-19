#include "session_queue.h"
/* At the end of input WaitForChar answers true: a Read then returns at once
 * with 0 and the program learns the window is gone. Answering false made a
 * closed window look like an expired timeout, and a program that polls
 * before it reads (a Rust event loop, Helix) waited for good. */
int session_queue_wait(const XSessionQueue *q,int eof,int expired)
{ return q->length || eof ? 1 : (expired ? 0 : -1); }
size_t session_queue_write(XSessionQueue *q,const unsigned char *b,size_t n)
{
    size_t i;
    if(n>XSESSION_CAPACITY-q->length) n=XSESSION_CAPACITY-q->length;
    for(i=0;i<n;++i) q->bytes[(q->head+q->length+i)%XSESSION_CAPACITY]=b[i];
    q->length+=n;
    return n;
}
size_t session_queue_read(XSessionQueue *q,unsigned char *b,size_t n)
{
    size_t i;
    if(n>q->length) n=q->length;
    for(i=0;i<n;++i) b[i]=q->bytes[(q->head+i)%XSESSION_CAPACITY];
    q->head=(q->head+n)%XSESSION_CAPACITY; q->length-=n;
    return n;
}
