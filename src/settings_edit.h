#ifndef XTERM_SETTINGS_EDIT_H
#define XTERM_SETTINGS_EDIT_H
#include "settings.h"
#include <stddef.h>
/* Limpet Preferences writes only the session-start keys of Limpet.conf.
 * Every other line (font, theme, comments, a line it does not understand)
 * stays as it was, in its place. A session key takes the place of its old
 * line, or goes at the end; one at its default value is left out, so the
 * file stays as short as before when nothing is chosen. Returns the new
 * text's length, or 0 if it would not fit in capacity or exceed
 * XSETTINGS_FILE_MAX. */
size_t settings_edit_session(const char *old, size_t old_length, const XSettings *, char *out, size_t capacity);
#endif
