#ifndef TITLE_BUFFER_H
#define TITLE_BUFFER_H
/* Two buffers for the window title. SetWindowTitles does not copy the
 * string, and Intuition draws the title bar in input.device's task, so the
 * buffer it holds may be read at any moment: it must not be written. A new
 * title is composed in the other buffer and handed over with
 * SetWindowTitles, which swaps the pointer under the window's layer locks;
 * after it returns the old buffer is no longer read and becomes the next
 * one to compose in. Both live as long as the program (static storage). */
#include <string.h>

typedef struct {
    char text[2][160];
    int shown;
} XTitleBuffers;

static inline void title_buffers_init(XTitleBuffers *t)
{ memset(t,0,sizeof *t); }

static inline const char *title_buffers_shown(const XTitleBuffers *t)
{ return t->text[t->shown]; }

/* The buffer to compose in: never the one Intuition holds. */
static inline char *title_buffers_next(XTitleBuffers *t)
{ return t->text[!t->shown]; }

/* Nonzero when the composed text differs and is now the shown buffer; the
 * caller then passes title_buffers_shown() to SetWindowTitles. */
static inline int title_buffers_commit(XTitleBuffers *t)
{
    if(!strcmp(t->text[!t->shown],t->text[t->shown])) return 0;
    t->shown=!t->shown;
    return 1;
}
#endif
