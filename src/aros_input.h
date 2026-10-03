#ifndef XTERM_AROS_INPUT_H
#define XTERM_AROS_INPUT_H
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <stddef.h>

/* Holds the dead-key union by value; it never borrows the IntuiMessage. */
typedef struct { struct InputEvent event; } ArosRawKey;
/* Call while message is alive, BEFORE ReplyMsg. Non-RAWKEY/key-up returns 0.
 * Successful capture can be encoded safely after ReplyMsg. */
int aros_input_capture(ArosRawKey *key, const struct IntuiMessage *message);
/* Calls default keymap.library MapRawKey for text. Left Alt is terminal Meta
 * (ESC prefix); Right Alt stays in the keymap for alternate Latin-1 glyphs.
 * Both Alt keys count as Alt for VT special-key modifiers. Shift/caps and
 * dead-key composition are keymap-owned; Ctrl is applied by terminal encoder.
 * At most 255 legacy bytes/event, 511 UTF-8/Meta bytes. A short output buffer
 * returns 0 and remains untouched. F1/F2 reservation belongs to the GUI. */
#define AROS_INPUT_MAX_BYTES 511
size_t aros_input_encode(const ArosRawKey *key, unsigned char *output,
                         size_t capacity);
#endif
