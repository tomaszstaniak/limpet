#ifndef XTERM_ECHO_SESSION_H
#define XTERM_ECHO_SESSION_H
#include <stddef.h>

#define ECHO_SESSION_CAPACITY 4096
typedef struct {
    unsigned char bytes[ECHO_SESSION_CAPACITY];
    size_t head, length;
    int open;
} EchoSession;

/* Init opens an empty session, including after close. Close discards pending
 * output and rejects reads/writes until init. No allocation or screen access. */
void echo_session_init(EchoSession *session);
void echo_session_close(EchoSession *session);
/* Returns INPUT bytes consumed; retry unconsumed suffix after draining.
 * Each byte's expansion is atomic: CR -> CR LF; BS/DEL -> BS space BS.
 * Everything else, including VT sequences, is copied byte for byte.
 * Backspace is a one-cell echo convenience, not grapheme/line editing. */
size_t echo_session_write(EchoSession *session, const unsigned char *input,
                          size_t length);
size_t echo_session_read(EchoSession *session, unsigned char *output,
                         size_t capacity);
#endif
