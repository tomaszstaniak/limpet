#include "margin_fill.h"

static uint32_t edge_bg(const XTermScreen *s, const XTheme *t, unsigned row, unsigned col)
{
    XTermCell cell;
    if (!s->alternate_active) return t->background;
    cell = screen_view_cell(s, row, col);
    cell.colour_flags &= ~XCOLOUR_SELECTED;
    return theme_bg(t, &cell);
}

static int add(const MarginGeometry *geometry, MarginRect *out, size_t capacity, size_t *n,
               unsigned x0, unsigned y0, unsigned x1, unsigned y1, uint32_t rgb)
{
    /* A window smaller than the grid: clip, and drop what lies outside. */
    if (x0 >= geometry->window_width || y0 >= geometry->window_height) return 1;
    if (x1 >= geometry->window_width) x1 = geometry->window_width - 1;
    if (y1 >= geometry->window_height) y1 = geometry->window_height - 1;
    if (x1 < x0 || y1 < y0) return 1;
    /* Merge with the previous rectangle of the same band and colour. */
    if (*n) {
        MarginRect *p = &out[*n - 1];
        if (p->rgb == rgb && p->y0 == y0 && p->y1 == y1 && p->x1 + 1 == x0) { p->x1 = x1; return 1; }
        if (p->rgb == rgb && p->x0 == x0 && p->x1 == x1 && p->y1 + 1 == y0) { p->y1 = y1; return 1; }
    }
    if (*n == capacity) return 0;
    out[*n].x0 = x0; out[*n].y0 = y0; out[*n].x1 = x1; out[*n].y1 = y1; out[*n].rgb = rgb;
    ++*n;
    return 1;
}

/* A horizontal band (top or bottom): left corner, one segment per column,
 * right corner, coloured from `row`. */
static int band(const XTermScreen *s, const XTheme *t, const MarginGeometry *g, unsigned row,
                unsigned y0, unsigned y1, unsigned grid_right, MarginRect *out, size_t capacity, size_t *n)
{
    unsigned col;
    if (!add(g, out, capacity, n, 0, y0, g->margin - 1, y1, edge_bg(s, t, row, 0))) return 0;
    for (col = 0; col < s->columns; ++col) {
        unsigned x0 = g->margin + col * g->cell_width;
        if (!add(g, out, capacity, n, x0, y0, x0 + g->cell_width - 1, y1, edge_bg(s, t, row, col))) return 0;
    }
    return add(g, out, capacity, n, grid_right, y0, g->window_width - 1, y1, edge_bg(s, t, row, s->columns - 1));
}

size_t margin_rects(const XTermScreen *s, const XTheme *t, const MarginGeometry *g,
                    MarginRect *out, size_t capacity)
{
    size_t n = 0;
    unsigned row, grid_right, grid_bottom;
    if (!s || !t || !g || !out || !s->rows || !s->columns || !g->margin ||
        !g->window_width || !g->window_height) return 0;
    grid_right = g->margin + s->columns * g->cell_width;
    grid_bottom = g->margin + s->rows * g->cell_height;
    if (!band(s, t, g, 0, 0, g->margin - 1, grid_right, out, capacity, &n)) return 0;
    /* All left segments, then all right ones, so a run of rows merges. */
    for (row = 0; row < s->rows; ++row) {
        unsigned y0 = g->margin + row * g->cell_height;
        if (!add(g, out, capacity, &n, 0, y0, g->margin - 1, y0 + g->cell_height - 1, edge_bg(s, t, row, 0)))
            return 0;
    }
    for (row = 0; row < s->rows && grid_right < g->window_width; ++row) {
        unsigned y0 = g->margin + row * g->cell_height;
        if (!add(g, out, capacity, &n, grid_right, y0, g->window_width - 1, y0 + g->cell_height - 1,
                 edge_bg(s, t, row, s->columns - 1)))
            return 0;
    }
    if (grid_bottom < g->window_height &&
        !band(s, t, g, s->rows - 1, grid_bottom, g->window_height - 1, grid_right, out, capacity, &n))
        return 0;
    return n;
}
