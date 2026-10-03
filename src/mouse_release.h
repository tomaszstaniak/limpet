#ifndef XTERM_MOUSE_RELEASE_H
#define XTERM_MOUSE_RELEASE_H
#include <stddef.h>
#include <string.h>
typedef struct XMouseRelease {
    unsigned count;
    size_t length[3];
    unsigned char bytes[3][64];
} XMouseRelease;
static inline int mouse_release_add(XMouseRelease *q,const unsigned char *p,size_t n) {
    if(!n || n>64 || q->count==3) return 0;
    memcpy(q->bytes[q->count],p,n);q->length[q->count]=n;++q->count;return 1;
}
/* Writer must accept the complete report or zero bytes, like console input. */
static inline void mouse_release_flush(XMouseRelease *q,
    size_t (*write)(void *,const unsigned char *,size_t),void *context) {
    while(q->count) {
        if(write(context,q->bytes[0],q->length[0])!=q->length[0]) return;
        --q->count;
        memmove(q->bytes,q->bytes+1,q->count*sizeof q->bytes[0]);
        memmove(q->length,q->length+1,q->count*sizeof q->length[0]);
    }
}
#endif
