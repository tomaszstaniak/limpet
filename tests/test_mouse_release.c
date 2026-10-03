#include "../src/mouse_release.h"
#include "../src/session_queue.h"
#include <assert.h>
#include <stdio.h>
static size_t atomic_write(void *context,const unsigned char *p,size_t n) {
    XSessionQueue *q=context;
    if(XSESSION_CAPACITY-q->length<n) return 0;
    return session_queue_write(q,p,n);
}
int main(void) {
    XMouseRelease pending={0}; XSessionQueue input={0};
    unsigned char bytes[XSESSION_CAPACITY];
    const unsigned char release[]="\033[<0;12;4m";
    memset(bytes,'x',sizeof bytes);
    assert(session_queue_write(&input,bytes,sizeof bytes)==sizeof bytes);
    assert(mouse_release_add(&pending,release,sizeof release-1));
    mouse_release_flush(&pending,atomic_write,&input);
    assert(pending.count==1 && input.length==XSESSION_CAPACITY);
    assert(session_queue_read(&input,bytes,3)==3);
    mouse_release_flush(&pending,atomic_write,&input);
    assert(pending.count==1 && input.length==XSESSION_CAPACITY-3);
    assert(session_queue_read(&input,bytes,sizeof bytes)==sizeof bytes-3);
    mouse_release_flush(&pending,atomic_write,&input);
    assert(!pending.count);
    assert(session_queue_read(&input,bytes,sizeof bytes)==sizeof release-1);
    assert(!memcmp(bytes,release,sizeof release-1));
    mouse_release_flush(&pending,atomic_write,&input);
    assert(!input.length);
    assert(mouse_release_add(&pending,(const unsigned char *)"a",1));
    assert(mouse_release_add(&pending,(const unsigned char *)"b",1));
    assert(mouse_release_add(&pending,(const unsigned char *)"c",1));
    assert(!mouse_release_add(&pending,(const unsigned char *)"d",1));
    mouse_release_flush(&pending,atomic_write,&input);
    assert(session_queue_read(&input,bytes,sizeof bytes)==3 && !memcmp(bytes,"abc",3));
    assert(!mouse_release_add(&pending,bytes,65));
    puts("mouse release PASS: bounded FIFO, full queue retry, no partial/duplicate release");
}
