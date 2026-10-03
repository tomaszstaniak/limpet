#include "session_queue.h"
int session_queue_wait(const XSessionQueue *q,int eof,int expired)
{ return q->length ? 1 : (eof || expired ? 0 : -1); }
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
