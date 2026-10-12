#ifndef XTERM_FILE_DROP_H
#define XTERM_FILE_DROP_H
#include <stddef.h>
#include "line_editor.h"
/* Paths dropped from Workbench, written as AROS Shell arguments into the
 * native command line (docs/file-drop.md). Pure: no DOS calls. */
#define XDROP_MAX_FILES 64
typedef enum {
    XDROP_OK,
    XDROP_BAD_NAME,     /* a control character, or not UTF-8 text */
    XDROP_TOO_LONG,     /* the line would exceed XLINE_CAPACITY-1 bytes */
    XDROP_TOO_MANY,     /* more than XDROP_MAX_FILES objects */
    XDROP_SEARCHING,    /* a history search (Ctrl-R) is open */
    XDROP_SHELL_CUT     /* the Shell would end the line early at a ; (below) */
} XDropResult;
/* One path as one argument: as it is, or in double quotes with *" ** *$ *`
 * for the characters the Shell or ReadArgs would read. Writes a NUL.
 * Returns the length, or 0 when the path is empty, holds a control
 * character or invalid UTF-8, or does not fit capacity. */
size_t file_drop_quote(const char *path, char *out, size_t capacity);
/* Whether the Shell would take a ; for the start of a comment where the
   quoting says it is text. The ABIv11 Shell's readLine flips its quote
   state at every ", the escaped *" included, so after a name with a quote
   a quoted ; later on the line ends it (workbench/c/shell/readLine.c). */
int file_drop_shell_cuts(const char *line, size_t length);
/* All paths at the cursor, in order, spaced from the text around them; a
 * selection stays as text. All or nothing: on any error the editor is
 * unchanged. *failed gets the index of a path that cannot be written
 * (XDROP_BAD_NAME), or count. */
XDropResult file_drop_insert(XLineEditor *, const char *const *paths, size_t count, size_t *failed);
#endif
