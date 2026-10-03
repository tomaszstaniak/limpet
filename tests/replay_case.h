#ifndef AROS_XTERM_REPLAY_CASE_H
#define AROS_XTERM_REPLAY_CASE_H
#include "../src/screen_model.h"

typedef struct ReplaySnapshot {
    unsigned rows, columns, cursor_row, cursor_column;
    unsigned history_count, view_offset, wrap_pending;
    XTermCell style;
    const XTermCell *cells;
} ReplaySnapshot;

typedef struct ReplayAction {
    unsigned operation; /* 1 = resize to snapshot dimensions; 2 = scroll. */
    int argument;
    ReplaySnapshot expected;
} ReplayAction;

typedef struct ReplayCase {
    const char *name;
    const unsigned char *bytes;
    size_t length;
    unsigned history_lines;
    ReplaySnapshot initial;
    const ReplayAction *actions;
    size_t action_count;
} ReplayCase;
#endif

