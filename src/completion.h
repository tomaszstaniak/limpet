#ifndef XTERM_COMPLETION_H
#define XTERM_COMPLETION_H
#include "line_editor.h"
#define XCOMP_MAX 128
/* Bounded, caller-owned TAB transaction. Names are decoded UTF-8; matching
 * folds ASCII only. Never evaluates Shell expressions or filesystem patterns. */
typedef struct {
    char directory[XLINE_CAPACITY], prefix[XLINE_CAPACITY];
    char names[XCOMP_MAX][XLINE_CAPACITY];
    char expected[XLINE_CAPACITY];
    size_t start, end, cursor, count, next;
    int command, quoted, limited, applied;
} XCompletion;
int completion_prepare(XCompletion *, const XLineEditor *);
int completion_add(XCompletion *, const char *name, int kind);
/* kind: 0 file/command, 1 directory ('/'), 2 volume/device/assign (':'). */
int completion_apply(XCompletion *, XLineEditor *, int reverse);
int completion_current(const XCompletion *, const XLineEditor *);
#endif
