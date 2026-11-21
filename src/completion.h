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
    size_t words;   /* the first `words` names are a command's keywords and
                       allowed words: offered before files, in their own order */
    int command, quoted, limited, applied;
} XCompletion;
int completion_prepare(XCompletion *, const XLineEditor *);
int completion_add(XCompletion *, const char *name, int kind);
/* kind: 0 file/command, 1 directory ('/'), 2 volume/device/assign (':'). */
/* A keyword or allowed word of the command (completion_args.h): sorted
 * among the words, ahead of every file. Add these before the files. */
int completion_add_word(XCompletion *, const char *word);
int completion_apply(XCompletion *, XLineEditor *, int reverse);
int completion_current(const XCompletion *, const XLineEditor *);
#endif
