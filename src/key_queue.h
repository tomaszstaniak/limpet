#ifndef XTERM_KEY_QUEUE_H
#define XTERM_KEY_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#define XKEY_QUEUE_CAPACITY 256
#define XKEY_BYTES 511
typedef struct {
    uint16_t code, qualifiers;
    size_t length, offset;
    unsigned char bytes[XKEY_BYTES];
} XQueuedKey;
typedef struct {
    XQueuedKey events[XKEY_QUEUE_CAPACITY];
    unsigned head, count;
} XKeyQueue;
/* Caller-owned, zero-initialized, no borrowed event/keymap pointers. */
int key_queue_add(XKeyQueue *,uint16_t,uint16_t,const unsigned char *,size_t);
void key_queue_clear(XKeyQueue *);
/* Callback: 0 blocked (retain head), 1 done, -1 explicitly rejected.
 * It may advance offset after a partial write, but must never reorder input.
 * Returns number removed; bounded budget prevents GUI starvation. */
unsigned key_queue_drain(XKeyQueue *,unsigned,int (*)(void *,XQueuedKey *),void *);
#endif
