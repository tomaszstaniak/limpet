#include "reply_flow.h"
#include <assert.h>
#include <string.h>
static void emit(void *context,const unsigned char *p,size_t n)
{
    XReplyFlow *q=context;
    /* One input byte can finish at most one model response. The pump stops
     * before parsing another byte if this response cannot be delivered. */
    assert(!q->length && n<=sizeof q->bytes);
    memcpy(q->bytes,p,n); q->length=n; q->offset=0;
}
static int flush(XReplyFlow *q,
                 size_t (*send)(void *,const unsigned char *,size_t),void *context)
{
    size_t n;
    if(!q->length) return 1;
    n=send(context,q->bytes+q->offset,q->length-q->offset);
    assert(n<=q->length-q->offset);
    q->offset+=n;
    if(q->offset!=q->length) return 0;
    q->length=q->offset=0; return 1;
}
size_t reply_flow_write(XReplyFlow *q,XTermScreen *s,const unsigned char *p,size_t n,
                       size_t (*send)(void *,const unsigned char *,size_t),void *context)
{
    size_t consumed=0;
    void (*saved_reply)(void *,const unsigned char *,size_t)=s->reply;
    void *saved_context=s->reply_context;
    if(!flush(q,send,context)) return 0;
    s->reply=emit; s->reply_context=q;
    while(consumed<n) {
        /* Image payload cannot answer, so it need not go byte by byte. */
        size_t bulk=screen_bulk_length(s,p+consumed,n-consumed);
        if(bulk) { screen_write(s,p+consumed,bulk); consumed+=bulk; continue; }
        screen_write(s,p+consumed,1); ++consumed;
        if(!flush(q,send,context)) break;
    }
    s->reply=saved_reply; s->reply_context=saved_context;
    return consumed;
}
