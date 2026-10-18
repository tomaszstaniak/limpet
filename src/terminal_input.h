#ifndef XTERM_TERMINAL_INPUT_H
#define XTERM_TERMINAL_INPUT_H

#include <stddef.h>

enum { TERMINAL_SHIFT = 1, TERMINAL_ALT = 2, TERMINAL_CTRL = 4 };
typedef enum {
    TERMINAL_KEY_NONE, TERMINAL_KEY_ESCAPE, TERMINAL_KEY_ENTER,
    TERMINAL_KEY_BACKSPACE, TERMINAL_KEY_TAB,
    TERMINAL_KEY_UP, TERMINAL_KEY_DOWN, TERMINAL_KEY_RIGHT, TERMINAL_KEY_LEFT,
    TERMINAL_KEY_HOME, TERMINAL_KEY_END, TERMINAL_KEY_INSERT,
    TERMINAL_KEY_DELETE, TERMINAL_KEY_PAGEUP, TERMINAL_KEY_PAGEDOWN,
    TERMINAL_KEY_F1, TERMINAL_KEY_F2, TERMINAL_KEY_F3, TERMINAL_KEY_F4,
    TERMINAL_KEY_F5, TERMINAL_KEY_F6, TERMINAL_KEY_F7, TERMINAL_KEY_F8,
    TERMINAL_KEY_F9, TERMINAL_KEY_F10, TERMINAL_KEY_F11, TERMINAL_KEY_F12
} TerminalKey;

/* VT normal-cursor mode. Backspace emits DEL; Enter emits CR. Encoding is
 * atomic: return 0 without modifying output if capacity is insufficient. */
size_t terminal_encode_key(TerminalKey key, unsigned modifiers,
                           unsigned char *output, size_t capacity);
/* Enter with modifiers in xterm's modifyOtherKeys form, for a client that
 * turned it on with CSI > 4 ; 1 m. Only Shift+Enter is sent this way
 * (CSI 27;2;13~); 0 for anything else, which then stays CR. Atomic as above. */
size_t terminal_encode_modified_enter(unsigned modifiers, unsigned char *output, size_t capacity);
/* Explicit Latin-1 default, not a Unicode keymap decoder. Shift/caps have
 * already been applied by the keymap. Ctrl maps ASCII @.._ and a..z,
 * space/2 to NUL and ? to DEL; Alt prefixes one ESC per mapped event. */
size_t terminal_encode_latin1(const unsigned char *bytes, size_t length,
                              unsigned modifiers, unsigned char *output,
                              size_t capacity);
#endif
