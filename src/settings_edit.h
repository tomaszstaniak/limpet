#ifndef XTERM_SETTINGS_EDIT_H
#define XTERM_SETTINGS_EDIT_H
#include "settings.h"
#include <stddef.h>
/* Limpet Preferences writes the session-start keys into Session.conf. Every
 * other line (comments, a line it does not understand) stays as it was, in
 * its place. A key takes the place of its old line, or goes at the end; one
 * at its default value is left out, so with nothing chosen the file may
 * become empty. Returns 1 with the new text and its length in *length
 * (NUL-terminated), or 0 if it would not fit in capacity or exceed
 * XSETTINGS_FILE_MAX. */
int settings_edit_session(const char *old, size_t old_length, const XSettings *, char *out, size_t capacity,
                          size_t *length);
#endif
