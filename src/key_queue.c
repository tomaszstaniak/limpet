#include "key_queue.h"
#include <string.h>
int key_queue_add(XKeyQueue *q,uint16_t code,uint16_t qualifiers,const unsigned char *bytes,size_t n)
{
    XQueuedKey *key;
    if(n>XKEY_BYTES || (n && !bytes) || q->count==XKEY_QUEUE_CAPACITY) return 0;
    key=&q->events[(q->head+q->count)%XKEY_QUEUE_CAPACITY];
    key->code=code; key->qualifiers=qualifiers; key->length=n; key->offset=0;
    if(n) memcpy(key->bytes,bytes,n);
    ++q->count; return 1;
}
void key_queue_clear(XKeyQueue *q) { q->head=q->count=0; }
unsigned key_queue_drain(XKeyQueue *q,unsigned budget,int (*send)(void *,XQueuedKey *),void *context)
{
    unsigned removed=0;
    while(q->count && removed<budget) {
        if(!send(context,&q->events[q->head])) break;
        q->head=(q->head+1)%XKEY_QUEUE_CAPACITY;
        --q->count; ++removed;
    }
    return removed;
}
