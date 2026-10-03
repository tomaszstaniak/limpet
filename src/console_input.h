#ifndef XTERM_CONSOLE_INPUT_H
#define XTERM_CONSOLE_INPUT_H
#include "session_queue.h"
#define XCONSOLE_LINE_CAPACITY 1024
typedef struct {
    unsigned char line[XCONSOLE_LINE_CAPACITY];
    size_t length;
    int raw, skip_lf, escape;
} XConsoleInput;
/* Consume one byte atomically, or return zero when queues cannot accept it.
 * Cooked mode supports append/ASCII erase; VT navigation sequences are ignored.
 * UTF-8 is transported intact; non-ASCII suffix deletion is intentionally absent. */
int console_input_byte(XConsoleInput *, XSessionQueue *, XSessionQueue *, unsigned char);
/* Reject a mode change while an unfinished cooked line exists. */
int console_input_mode(XConsoleInput *, int raw);
/* Frontend editor path: stage the cooked line without echo, then submit LF.
 * These calls never accept raw input; submission is atomic under backpressure. */
int console_input_stage(XConsoleInput *, const char *, size_t);
int console_input_submit(XConsoleInput *, XSessionQueue *);
#endif
