#include "../src/reply_flow.h"
#include "../src/session_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen screen;
static XTermCell cells[40];
static XReplyFlow flow;
static XSessionQueue input;
static size_t send(void *context,const unsigned char *p,size_t n)
{ return session_queue_write(context,p,n); }
static void init(void)
{
    memset(&flow,0,sizeof flow); memset(&input,0,sizeof input);
    assert(!screen_init(&screen,cells,40,4,10));
}
/* Reproduce ignored short writes with the real parser and bounded input ring.
 * Dropping a reply or parsing beyond it changes either consumption or bytes. */
int main(void)
{
    const unsigned char stream[]="\033[6nX\033[6nY";
    unsigned char out[XSESSION_CAPACITY],fill[XSESSION_CAPACITY];
    size_t used,n,i;
    memset(fill,'!',sizeof fill); init();
    assert(session_queue_write(&input,fill,sizeof fill)==sizeof fill);
    used=reply_flow_write(&flow,&screen,stream,sizeof stream-1,send,&input);
    assert(used==4 && flow.length==6 && flow.offset==0);
    assert(screen_cell(&screen,0,0)==' ');
    assert(!reply_flow_write(&flow,&screen,stream+used,sizeof stream-1-used,send,&input));
    assert(session_queue_read(&input,out,sizeof out)==sizeof out);
    assert(!memcmp(out,fill,sizeof out));
    n=reply_flow_write(&flow,&screen,stream+used,sizeof stream-1-used,send,&input);
    assert(n==sizeof stream-1-used && !flow.length);
    assert(screen_cell(&screen,0,0)=='X' && screen_cell(&screen,0,1)=='Y');
    assert(session_queue_read(&input,out,sizeof out)==12);
    assert(!memcmp(out,"\033[1;1R\033[1;2R",12));
    /* Partial writer: one slot freed per retry must never duplicate a prefix. */
    init(); session_queue_write(&input,fill,sizeof fill-1);
    assert(reply_flow_write(&flow,&screen,stream,4,send,&input)==4);
    assert(flow.offset==1 && flow.length==6);
    for(i=0;i<5;++i) {
        assert(session_queue_read(&input,out,1)==1 && out[0]=='!');
        assert(!reply_flow_write(&flow,&screen,NULL,0,send,&input));
    }
    assert(!flow.length);
    assert(session_queue_read(&input,out,sizeof out)==sizeof out);
    assert(!memcmp(out+sizeof out-6,"\033[1;1R",6));
    /* Ownership changes cancel pending replies, not the retained output tail. */
    init(); session_queue_write(&input,fill,sizeof fill);
    assert(reply_flow_write(&flow,&screen,stream,sizeof stream-1,send,&input)==4);
    memset(&flow,0,sizeof flow); /* same explicit cancellation as frontend */
    screen_write(&screen,stream+4,sizeof stream-1-4);
    assert(!screen.reply && !screen.reply_context);
    assert(session_queue_read(&input,out,sizeof out)==sizeof out);
    assert(!memcmp(out,fill,sizeof out));
    assert(screen_cell(&screen,0,0)=='X' && screen_cell(&screen,0,1)=='Y');
    /* A query begun without a reply owner cannot gain one at its final byte. */
    init();
    screen_write(&screen,(const unsigned char *)"\033[6",3);
    assert(reply_flow_write(&flow,&screen,(const unsigned char *)"nZ",2,send,&input)==2);
    assert(!input.length && screen_cell(&screen,0,0)=='Z');
    init(); screen_write(&screen,(const unsigned char *)"\033",1);
    assert(reply_flow_write(&flow,&screen,(const unsigned char *)"[6nZ",4,send,&input)==4);
    assert(!input.length && screen_cell(&screen,0,0)=='Z');
    puts("reply flow PASS: full ring, retained suffix, ordered replies, partial writes, cancellation");
    return 0;
}
