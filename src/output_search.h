#ifndef XTERM_OUTPUT_SEARCH_H
#define XTERM_OUTPUT_SEARCH_H
#include "screen_model.h"
/* Search of the terminal's own text: the main screen's scrollback and its
 * rows (not the command history; that is Ctrl-R in line_editor.h).
 *
 * The query is literal text, matched scalar by scalar against the cells,
 * ignoring the case of ASCII letters only. A match starts and ends on whole
 * cells, so it never takes part of a cluster. Soft-wrapped rows are one
 * line; a hard line end is never crossed. Nothing is normalised: a composed
 * and a decomposed letter are different text.
 *
 * Positions are absolute lines of the screen model's current layout
 * (history_pushed), so arriving output and lines falling out of the history
 * do not move a result. After a layout change (resize, reflow) the current
 * result is found again by its place counted from the newest match, which
 * a reflow does not change.
 *
 * While open, the view does not follow output: it keeps showing the same
 * text until the search closes. The alternate screen is not searched: the
 * text a full-screen program shows is its own, and what it scrolled away
 * is only in that program. */
#define XSEARCH_QUERY_MAX 128
#define XSEARCH_SCALARS 64

typedef struct {
    int open;
    char query[XSEARCH_QUERY_MAX];
    size_t query_length;
    uint32_t scalars[XSEARCH_SCALARS];
    unsigned scalar_count;
    /* The current result: start line (absolute) and column, cells covered. */
    int found;
    unsigned long line;
    unsigned column, cells;
    unsigned long index, total;     /* 1-based from the oldest; total matches */
    /* The view before the search, for Esc. */
    unsigned restore_offset;
    unsigned long restore_pushed;
    /* What the model looked like at the last look. */
    unsigned long pushed_seen, layout_seen, visible_seen;
    unsigned offset_seen;
} XOutputSearch;

typedef enum { XSEARCH_OPENED, XSEARCH_ALTERNATE, XSEARCH_NO_HISTORY } XSearchOpen;
XSearchOpen output_search_open(XOutputSearch *, XTermScreen *);
/* Complete UTF-8 scalars without controls go to the end of the query; the
 * search then runs again from the current result (inclusive) towards older
 * text. Returns 0, changing nothing, for invalid or overflowing input. */
int output_search_insert(XOutputSearch *, XTermScreen *, const char *, size_t);
/* Removes the last scalar; searches again from where the view was when the
 * search opened. Returns 0 for an empty query. */
int output_search_backspace(XOutputSearch *, XTermScreen *);
/* To the next older (older=1) or newer result; 0 at the oldest or newest. */
int output_search_step(XOutputSearch *, XTermScreen *, int older);
/* restore=1: back to the view from before (Esc), else stay where it is. */
void output_search_close(XOutputSearch *, XTermScreen *, int restore);
/* Call after output, a resize or a scroll of the view while open. Returns
 * 1 when the search closed itself: the alternate screen is now shown. */
int output_search_follow(XOutputSearch *, XTermScreen *);
/* For each row shown (rows*columns cells, row-major): 0, 1 for a cell of
 * some match, 2 for a cell of the current one. */
void output_search_marks(const XOutputSearch *, const XTermScreen *,
                         unsigned char *marks, size_t capacity);
/* "Find: query  3 of 17", "Find: query  not found"; cut at a scalar. */
size_t output_search_label(const XOutputSearch *, char *out, size_t capacity);
#endif
