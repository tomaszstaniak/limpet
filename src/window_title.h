#ifndef XTERM_WINDOW_TITLE_H
#define XTERM_WINDOW_TITLE_H
/* The window title says what runs in the window, then what the terminal
 * has to report:
 *
 *     <subject> - <terminal>[ | <status>][ | scrollback a/b]
 *
 * subject, first that applies: a link under the pointer ("Link: url"),
 * the title the running program set (OSC 0/2), the running command's name,
 * "Shell" for an idle Shell session; none otherwise. The status comes after
 * the name so that a narrow title bar cuts the status, not the name.
 *
 * Output is in the title bar's character set, Latin-1 (as the clipboard's
 * legacy fallback): program titles arrive as UTF-8 and characters outside
 * Latin-1 become '?'; command names are already in the system character
 * set and only lose control characters. Plain ASCII hyphen: Latin-1 has no
 * dash. Portable C, no AROS calls, host-tested. */
#include <stddef.h>

typedef struct {
    const char *terminal;   /* "Limpet" */
    const char *link;       /* URL under the pointer, or NULL */
    const char *program;    /* OSC title of the running command (UTF-8), or NULL/"" */
    const char *command;    /* task name of the running command, or NULL */
    int shell;              /* a Shell session: "Shell" when nothing runs */
    const char *status;     /* terminal message, or NULL */
    unsigned view_offset, history_count; /* scrollback position, 0: live */
} XWindowTitle;

/* Writes a NUL-terminated title of at most size-1 bytes (cut at a byte:
 * Latin-1 has no multi-byte characters). Returns its length. */
size_t window_title_compose(char *out, size_t size, const XWindowTitle *in);

/* What runs in the foreground of a Shell session, as the terminal can read
 * it from the Shell's process: whether a command runs (the CLI's module is
 * set) and the name the Shell gave its task meanwhile. known is 0 when
 * nothing could be read (no reader yet, the reader gone, not a CLI). */
typedef struct {
    int known, running;
    unsigned long module;     /* the command's segment, as an identity only */
    char name[64];
} XForeground;

/* A program's title belongs to the command that was in the foreground when
 * the title arrived and ends with it: when the foreground changes (another
 * command, back to the prompt) the title is dropped. A title that arrived
 * while nothing could be read is kept until the session ends or the program
 * sets another one: there is nothing to tie it to. */
typedef struct {
    unsigned long serial;     /* screen title_serial last taken */
    int set, owner_known, owner_running;
    unsigned long owner_module;
    char text[256];
} XProgramTitle;

/* Take a new OSC title (when serial differs) for the given foreground. */
void program_title_offer(XProgramTitle *, const char *title, unsigned long serial,
                         const XForeground *);
/* Drop the title when the foreground is no longer its owner. Returns 1
 * when it was dropped. */
int program_title_follow(XProgramTitle *, const XForeground *);
/* The title to show, or NULL. */
const char *program_title_text(const XProgramTitle *);
/* Forget the title (session end or restart); keep the serial. */
void program_title_clear(XProgramTitle *);

/* The file part of a command as the Shell names it ("Work:hc/hardcode",
 * "C:Wait" -> "hardcode", "Wait"). Points into name. */
const char *window_title_command_name(const char *name);
#endif
