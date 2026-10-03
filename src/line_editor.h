#ifndef XTERM_LINE_EDITOR_H
#define XTERM_LINE_EDITOR_H
#include <stddef.h>
#define XLINE_CAPACITY 1024
#define XLINE_HISTORY 32
#define XLINE_CLUSTER_LIMIT 16

/* Zero-initialize caller-owned storage. UTF-8 byte offsets are always extended
 * grapheme boundaries. This model has no terminal geometry, echo or DOS calls. */
typedef struct {
    char text[XLINE_CAPACITY];
    size_t length, cursor;
    size_t anchor;
    int selecting;
    char history[XLINE_HISTORY][XLINE_CAPACITY];
    size_t count, head, browsing;
    char draft[XLINE_CAPACITY];
    size_t draft_cursor;
    size_t draft_anchor;
    int draft_selecting;
} XLineEditor;
typedef enum {
    XLINE_LEFT, XLINE_RIGHT, XLINE_HOME, XLINE_END,
    XLINE_BACKSPACE, XLINE_DELETE, XLINE_PREVIOUS, XLINE_NEXT, XLINE_CLEAR,
    XLINE_SELECT_LEFT, XLINE_SELECT_RIGHT, XLINE_SELECT_HOME, XLINE_SELECT_END
} XLineAction;
/* Complete UTF-8 spans only; rejects controls, malformed/incomplete encoding,
 * clusters over 16 scalars and overflow atomically. Source may alias text.
 * Empty insertion succeeds without changing state. */
int line_editor_insert(XLineEditor *, const char *, size_t);
/* Returns whether an action applied (including history traversal); zero at
 * boundaries, for invalid actions or if an edit would exceed cluster bounds. */
int line_editor_action(XLineEditor *, XLineAction);
/* Output must not overlap editor storage. Commit writes text plus LF (no NUL)
 * only if capacity suffices, then records nonempty history and clears input.
 * Returns bytes written, or zero with state/output unchanged. */
size_t line_editor_commit(XLineEditor *, char *, size_t);
#endif
