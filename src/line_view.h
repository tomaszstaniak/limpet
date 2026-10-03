#ifndef XTERM_LINE_VIEW_H
#define XTERM_LINE_VIEW_H
#include "line_editor.h"
#include "screen_model.h"
/* Compose a temporary live view without modifying base cells/history. Scratch
 * storage must be disjoint; cells >= base capacity, history >= 2*XLINE_CAPACITY.
 * The resulting cursor is a viewport coordinate, even when command editing
 * scrolls the scratch view to keep the cursor visible. */
int line_view_build(XTermScreen *, const XTermScreen *, const XLineEditor *,
                    XTermCell *, size_t, XTermCell *, size_t);
#endif
