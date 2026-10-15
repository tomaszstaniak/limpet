#ifndef XTERM_LINE_EDITOR_H
#define XTERM_LINE_EDITOR_H
#include <stddef.h>
#define XLINE_CAPACITY 1024
#define XLINE_HISTORY 32
#define XLINE_CLUSTER_LIMIT 16
#define XLINE_QUERY_MAX 64

/* Zero-initialize caller-owned storage. UTF-8 byte offsets are always extended
 * grapheme boundaries. This model has no terminal geometry, echo or DOS calls. */
typedef struct {
    char text[XLINE_CAPACITY];
    size_t length, cursor;
    size_t anchor;
    int selecting;
    /* Commands run, a ring: the built-in XLINE_HISTORY entries, or the
     * caller's storage after line_editor_attach_history. */
    char history_builtin[XLINE_HISTORY][XLINE_CAPACITY];
    char (*history_store)[XLINE_CAPACITY];
    size_t history_capacity;
    size_t count, head, browsing;
    char draft[XLINE_CAPACITY];
    size_t draft_cursor;
    size_t draft_anchor;
    int draft_selecting;
    /* Reverse history search (Ctrl-R). While searching, text shows the
     * match (or what was there before) and the query lives apart. */
    int searching, search_failed;
    char query[XLINE_QUERY_MAX];
    size_t query_length;
    size_t match;               /* history age of the match shown, 1 newest; 0 none */
    char saved[XLINE_CAPACITY];
    size_t saved_length, saved_cursor, saved_anchor, saved_browsing;
    int saved_selecting;
} XLineEditor;
typedef enum {
    XLINE_LEFT, XLINE_RIGHT, XLINE_HOME, XLINE_END,
    XLINE_BACKSPACE, XLINE_DELETE, XLINE_PREVIOUS, XLINE_NEXT, XLINE_CLEAR,
    XLINE_SELECT_LEFT, XLINE_SELECT_RIGHT, XLINE_SELECT_HOME, XLINE_SELECT_END
} XLineAction;
/* Keep up to count commands in caller-owned storage instead of the built-in
 * XLINE_HISTORY; the history so far is dropped. NULL or 0 returns to the
 * built-in ring. Zeroing the editor (memset) also returns to it. */
void line_editor_attach_history(XLineEditor *, char (*entries)[XLINE_CAPACITY], size_t count);
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

/* Reverse search through the command history (not the scrollback).
 * Matching is a substring test on the UTF-8 bytes, ignoring ASCII case.
 * start: the first Ctrl-R remembers the line, cursor and selection and
 * starts with an empty query; while searching it moves to the next older
 * match whose text differs from the one shown. Returns whether a match is
 * shown. A query that matches nothing sets search_failed and leaves the
 * text shown as it was (the last match, or the line as typed). */
int line_editor_search_start(XLineEditor *);
/* Append complete UTF-8 scalars without controls to the query (at most
 * XLINE_QUERY_MAX-1 bytes) and search again from the match shown. Returns
 * 0, changing nothing, for invalid or overflowing input. */
int line_editor_search_insert(XLineEditor *, const char *, size_t);
/* Remove the query's last scalar and search again from the newest entry. */
int line_editor_search_backspace(XLineEditor *);
/* Keep the text shown for editing, cursor at its end; nothing is run. */
void line_editor_search_accept(XLineEditor *);
/* Back to the line, cursor and selection from before the search. */
void line_editor_search_cancel(XLineEditor *);
/* What the command line shows while searching:
 * "(reverse-i-search)`query': text", or "(failed reverse-i-search)..."
 * when nothing matches; the text is cut at a scalar to fit. *cursor gets
 * the byte offset of the match in the label (its end when there is none).
 * Returns the length, or 0 when not searching. */
size_t line_editor_search_label(const XLineEditor *, char *out, size_t capacity, size_t *cursor);
#endif
