#ifndef XTERM_SESSION_START_H
#define XTERM_SESSION_START_H
#include "settings.h"
#include <stddef.h>
/* The command line a new Shell window runs once, after S:Shell-Startup,
 * from the settings: nothing, Fastfetch in the chosen look (a preset of the
 * Fastfetch package, or the user's file, never a copy of either) with the
 * chosen groups and logo, or the user's own command as written. Limpet
 * Preferences shows and runs the same line. Returns 0 when nothing is to
 * run, 1 with the line (NUL-terminated, no line end) in out, or -1 with
 * *why saying what is missing or not allowed. */
int session_start_line(const XSettings *, char *out, size_t capacity, const char **why);
#endif
