#ifndef XTERM_MARGIN_FILL_H
#define XTERM_MARGIN_FILL_H
#include "theme.h"
#include <stddef.h>
#include <stdint.h>

/* The window area around the text grid: the fixed margin plus the pixels
 * left over when the window is not a whole number of cells.
 *
 * A full-screen program (alternate screen) colours every cell, and a margin
 * in the terminal's own background would frame it. There each margin takes
 * the background of the cells next to it: the left and right margins per
 * row, the top and bottom margins per column, each corner from its corner
 * cell. The colour is the cell's own (inverse applied, selection ignored),
 * never the cursor's. Otherwise the margin is the theme's background. */
typedef struct {
    unsigned x0, y0, x1, y1; /* inclusive, in window inner coordinates */
    uint32_t rgb;
} MarginRect;

typedef struct {
    unsigned cell_width, cell_height, margin; /* pixels */
    unsigned window_width, window_height;     /* the inner area */
} MarginGeometry;

/* Rectangles covering the whole area outside the grid, neighbouring cells of
 * one colour merged. Returns their number; 0 when `capacity` is too small
 * (MARGIN_RECTS_MAX always suffices for grids up to `max_rows` x `max_columns`). */
#define MARGIN_RECTS_MAX(max_rows, max_columns) (2u * ((max_columns) + 2u) + 2u * (max_rows))
size_t margin_rects(const XTermScreen *, const XTheme *, const MarginGeometry *,
                    MarginRect *out, size_t capacity);
#endif
