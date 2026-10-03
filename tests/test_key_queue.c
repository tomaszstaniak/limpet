#include "../src/key_queue.h"
#include "../src/line_editor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XKeyQueue queue;
static XLineEditor editor;
static int ready;
static int apply(void *context,XQueuedKey *key)
{
    XLineEditor *s=context;
    if(!ready) return 0;
    if(key->code==1) line_editor_action(s,XLINE_LEFT);
    else assert(line_editor_insert(s,(const char *)key->bytes,key->length));
    return 1;
}
static int partial(void *context,XQueuedKey *key)
{
    char *out=context;
    if(key->offset<key->length) {
        size_t n=strlen(out); out[n]=key->bytes[key->offset++]; out[n+1]=0;
        return 0;
    }
    return 1;
}
int main(void)
{
    unsigned i; char out[8]=""; unsigned char mutable[]="ab";
    /* The lost-key bug: no readiness must retain text AND following actions. */
    assert(key_queue_add(&queue,0,0,mutable,2));
    mutable[0]='Z';
    assert(key_queue_add(&queue,1,0,NULL,0));
    assert(key_queue_add(&queue,0,0,(const unsigned char *)"X",1));
    assert(!key_queue_drain(&queue,8,apply,&editor));
    assert(queue.count==3 && !editor.length);
    ready=1;
    assert(key_queue_drain(&queue,2,apply,&editor)==2);
    assert(queue.count==1 && !strcmp(editor.text,"ab"));
    assert(key_queue_drain(&queue,8,apply,&editor)==1);
    assert(!strcmp(editor.text,"aXb"));
    /* Partial transport admission neither repeats bytes nor overtakes head. */
    assert(key_queue_add(&queue,0,0,(const unsigned char *)"ab",2));
    assert(key_queue_add(&queue,0,0,(const unsigned char *)"C",1));
    for(i=0;i<5;++i) key_queue_drain(&queue,8,partial,out);
    assert(!queue.count && !strcmp(out,"abC"));
    /* Overflow preserves existing events; cancellation prevents stale replay. */
    for(i=0;i<XKEY_QUEUE_CAPACITY;++i)
        assert(key_queue_add(&queue,(uint16_t)i,7,(const unsigned char *)"x",1));
    assert(!key_queue_add(&queue,0,0,(const unsigned char *)"Y",1));
    assert(queue.count==XKEY_QUEUE_CAPACITY && queue.events[queue.head].code==0);
    key_queue_clear(&queue);
    assert(!key_queue_drain(&queue,8,apply,&editor));
    assert(!key_queue_add(&queue,0,0,NULL,1));
    assert(!key_queue_add(&queue,0,0,(const unsigned char *)"x",XKEY_BYTES+1));
    puts("key queue: PASS (blocked text/actions, ownership, partial writes, overflow, cancellation)");
    return 0;
}
