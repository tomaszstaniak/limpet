#ifndef XTERM_SELECTION_H
#define XTERM_SELECTION_H
#include "screen_model.h"
typedef struct {
    unsigned anchor_row, anchor_col, end_row, end_col;
    int active, dragging;
} XSelection;
typedef struct { int left,top; unsigned width,height; } XSelectionGrid;
int selection_hit(const XTermScreen *,const XSelectionGrid *,int x,int y,int clamp,unsigned *row,unsigned *col);
void selection_start(XSelection *, const XTermScreen *, unsigned row, unsigned col);
void selection_extend(XSelection *, const XTermScreen *, unsigned row, unsigned col);
int selection_contains(const XSelection *, const XTermScreen *, unsigned row, unsigned col);
/* UTF-8 without NUL; 0 success, -1 invalid/empty selection, -2 capacity.
 * Output untouched on error; written reset. Endpoints include touched clusters. */
int selection_text(const XSelection *, const XTermScreen *, unsigned char *, size_t, size_t *);
#endif
