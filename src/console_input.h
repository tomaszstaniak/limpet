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
/* Cooked Ctrl-C: whether to complete the interruption with an empty input
 * line. A reader waiting in Read (the Shell at its prompt, or a command
 * reading a line) is released by it; so is the Shell between writing its
 * prompt and sending the Read, which has no command module. A command that
 * is running and not reading gets only the break signal: an empty line
 * queued for it would be read by the Shell after its next prompt and print a
 * second one. Unknown foreground (no CLI to read) keeps the empty line, so a
 * reader can never be left waiting. */
int console_interrupt_needs_line(int read_pending,int foreground_known,int command_running);
#endif
