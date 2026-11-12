#ifndef XTERM_PREFS_PORT_H
#define XTERM_PREFS_PORT_H
/* Limpet Preferences and the window it was opened from. Limpet makes a
 * public port for that window ("Limpet.N") when Project > Preferences opens
 * the editor, and names it, with a key, in the editor's PORT and KEY
 * arguments: a closed window's name may be taken by a new one, its key
 * not. XPREFS_RUN
 * runs a line in that window's Shell now, as if it had been typed and
 * entered: only while the Shell waits at its prompt with nothing typed.
 * XPREFS_APPLY_WINDOW has the window read Window.conf again.
 * XPREFS_BACKGROUND asks what the window's screen can show, in background:
 * a window can be on any public screen, and the bits are all that crosses
 * here, not the Screen.
 * XPREFS_APPLY_SETTINGS has the window read its settings again, as Project >
 * Reload settings does. XPREFS_PREVIEW_THEME shows the theme named in line
 * (a theme= value) without changing any setting; an empty line returns to
 * the theme the window had. A Limpet up to 0.9.0 answers both with
 * XPREFS_BAD.
 * Limpet answers at once in result. Both sides are built from this tree;
 * magic and size guard against anything else sent to the port. */
#include <exec/ports.h>
#define XPREFS_PORT_PREFIX "Limpet."
#define XPREFS_MAGIC 0x4c507232UL /* "LPr2" */
#define XPREFS_LINE_MAX 512
enum { XPREFS_RUN = 1, XPREFS_APPLY_WINDOW = 2, XPREFS_BACKGROUND = 3,
       XPREFS_APPLY_SETTINGS = 4, XPREFS_PREVIEW_THEME = 5 };
enum { XPREFS_CAN_ALPHA = 1 };
enum {
    XPREFS_RAN,          /* the line went to the Shell */
    XPREFS_BUSY_PROGRAM, /* a command or full-screen program has the window */
    XPREFS_BUSY_TYPING,  /* a line is being typed, pasted or searched */
    XPREFS_NO_SHELL,     /* no Shell in this window (ended, or a command window) */
    XPREFS_BAD           /* not a request this window knows (another key: its window closed) */
};
typedef struct {
    struct Message message;
    ULONG magic, action, key;
    LONG result;
    ULONG background;
    char line[XPREFS_LINE_MAX];
} XPrefsRequest;
#endif
