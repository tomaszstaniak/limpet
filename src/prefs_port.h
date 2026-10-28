#ifndef XTERM_PREFS_PORT_H
#define XTERM_PREFS_PORT_H
/* Limpet Preferences and the window it was opened from. Limpet makes a
 * public port for that window ("Limpet.N") when Project > Preferences opens
 * the editor, and names it in the editor's PORT argument. The one request
 * runs a line in that window's Shell now, as if it had been typed and
 * entered: only while the Shell waits at its prompt with nothing typed.
 * Limpet answers at once in result. Both sides are built from this tree;
 * magic and size guard against anything else sent to the port. */
#include <exec/ports.h>
#define XPREFS_PORT_PREFIX "Limpet."
#define XPREFS_MAGIC 0x4c507231UL /* "LPr1" */
#define XPREFS_LINE_MAX 512
enum { XPREFS_RUN = 1 };
enum {
    XPREFS_RAN,          /* the line went to the Shell */
    XPREFS_BUSY_PROGRAM, /* a command or full-screen program has the window */
    XPREFS_BUSY_TYPING,  /* a line is being typed, pasted or searched */
    XPREFS_NO_SHELL,     /* no Shell in this window (ended, or a command window) */
    XPREFS_BAD           /* not a request this Limpet knows */
};
typedef struct {
    struct Message message;
    ULONG magic, action;
    LONG result;
    char line[XPREFS_LINE_MAX];
} XPrefsRequest;
#endif
