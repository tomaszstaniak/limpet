#ifndef XTERM_PASTE_H
#define XTERM_PASTE_H
#include <stddef.h>
#define PASTE_LIMIT 65536u
typedef struct {
    unsigned char bytes[PASTE_LIMIT];
    size_t length, offset;
    int confirmation;
} XPaste;
/* Reject invalid UTF-8, C0 except TAB/CR/LF, C1 and DEL atomically.
 * Normalize all line endings to CR; any newline requires confirmation.
 * 0 success, -1 invalid/control, -2 too large. Previous queue unchanged on error. */
int paste_prepare(XPaste *, const unsigned char *, size_t);
void paste_cancel(XPaste *);
void paste_confirm(XPaste *);
/* End on a UTF-8 scalar boundary. Use maximum >= 4 to guarantee progress;
 * consuming each complete returned batch makes cancellation scalar-safe. */
size_t paste_peek(const XPaste *, const unsigned char **, size_t maximum);
void paste_consume(XPaste *, size_t);
#endif
