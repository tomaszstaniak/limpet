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
/* Bracketed paste (DEC 2004): ESC[200~ before the text and ESC[201~ after,
 * queued so a session that takes part of a marker gets the rest next time.
 * paste_prepare already rejects ESC and C1 in the text, so a paste cannot
 * contain a marker of its own. */
typedef struct {
    unsigned char marker[8];
    size_t length, offset;
    int open;                       /* a start marker is queued or sent, the end is not */
    int in_band;                    /* from the first byte of ESC[200~ to the last of ESC[201~ */
} XPasteFrame;
void paste_frame_begin(XPasteFrame *, int bracketed);
void paste_frame_end(XPasteFrame *);
size_t paste_frame_peek(const XPasteFrame *, const unsigned char **);
void paste_frame_consume(XPasteFrame *, size_t);
#endif
