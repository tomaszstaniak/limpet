#ifndef XTERM_SESSION_QUEUE_H
#define XTERM_SESSION_QUEUE_H
#include <stddef.h>
#define XSESSION_CAPACITY 8192
typedef struct {
    unsigned char bytes[XSESSION_CAPACITY];
    size_t head, length;
} XSessionQueue;
size_t session_queue_write(XSessionQueue *, const unsigned char *, size_t);
size_t session_queue_read(XSessionQueue *, unsigned char *, size_t);
/* -1: keep waiting, 0: no character (timeout/EOF), 1: data available. */
int session_queue_wait(const XSessionQueue *, int eof, int expired);
#endif
