/* The window margin's colours: the theme background on the normal screen,
 * the edge cells' backgrounds on the alternate screen. */
#include "../src/margin_fill.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermScreen s;
static XTermCell cells[6*10], alt[6*10];
static XTheme t;
static MarginRect r[64];

static void out(const char *text) { screen_write(&s, (const unsigned char *)text, strlen(text)); }

/* The colour at window pixel (x, y), or 0xffffffff when no rectangle covers it;
 * also checks that no two rectangles overlap there. */
static uint32_t at(size_t n, unsigned x, unsigned y)
{
    uint32_t found = 0xffffffffu;
    size_t i;
    for (i = 0; i < n; ++i)
        if (x >= r[i].x0 && x <= r[i].x1 && y >= r[i].y0 && y <= r[i].y1) {
            assert(found == 0xffffffffu);
            found = r[i].rgb;
        }
    return found;
}

/* Every pixel outside the grid is covered, and none inside it. */
static void covers(size_t n, const MarginGeometry *g)
{
    unsigned x, y;
    for (y = 0; y < g->window_height; ++y)
        for (x = 0; x < g->window_width; ++x) {
            int grid = x >= g->margin && x < g->margin + s.columns * g->cell_width &&
                       y >= g->margin && y < g->margin + s.rows * g->cell_height;
            assert((at(n, x, y) == 0xffffffffu) == grid);
        }
}

int main(void)
{
    /* 6 rows x 10 columns of 8x16 cells, margin 12, and leftovers of 5 and 7 pixels. */
    MarginGeometry g = {8, 16, 12, 12 + 80 + 12 + 5, 12 + 96 + 12 + 7};
    size_t n;
    theme_default(&t);
    t.background = 0x101010; t.selection_bg = 0xabcdef;
    assert(!screen_init(&s, cells, 60, 6, 10));
    assert(!screen_attach_alternate(&s, alt, 60));

    /* Normal screen: the theme background everywhere, in four rectangles. */
    out("\033[48;2;200;0;0mred text");
    n = margin_rects(&s, &t, &g, r, 64);
    covers(n, &g);
    assert(n == 4 && at(n, 0, 0) == 0x101010 && at(n, g.window_width - 1, 50) == 0x101010);

    /* Alternate screen filled with one colour, like Helix: one colour, still four. */
    out("\033[?1049h\033[48;2;59;34;76m\033[2J");
    n = margin_rects(&s, &t, &g, r, 64);
    covers(n, &g);
    assert(n == 4 && at(n, 5, 5) == 0x3b224c && at(n, g.window_width - 1, g.window_height - 1) == 0x3b224c);

    /* A status line in another colour on the last row: the bottom band and the
     * side margins of that row take it, the rows above keep theirs. */
    out("\033[6;1H\033[48;2;0;80;0m\033[K");
    n = margin_rects(&s, &t, &g, r, 64);
    covers(n, &g);
    assert(at(n, 3, 12 + 5 * 16 + 4) == 0x005000);          /* left of row 6 */
    assert(at(n, g.window_width - 1, 12 + 5 * 16) == 0x005000); /* right of row 6 */
    assert(at(n, 40, g.window_height - 1) == 0x005000);      /* bottom band */
    assert(at(n, 3, 12 + 4 * 16) == 0x3b224c);                /* row 5 unchanged */
    assert(at(n, 40, 0) == 0x3b224c);                         /* top band */

    /* Per column at top and bottom, corners from corner cells. */
    out("\033[1;1H\033[48;2;1;2;3m \033[1;10H\033[48;2;4;5;6m ");
    n = margin_rects(&s, &t, &g, r, 64);
    covers(n, &g);
    assert(at(n, 0, 0) == 0x010203 && at(n, 12, 0) == 0x010203);  /* top-left corner and column 1 */
    assert(at(n, 20, 0) == 0x3b224c);                              /* column 2 */
    assert(at(n, g.window_width - 1, 0) == 0x040506);             /* top-right corner */
    assert(at(n, 0, 12) == 0x010203);                              /* left of row 1 */

    /* Inverse: the cell's background is its foreground colour. */
    out("\033[3;1H\033[0;38;2;9;9;9;48;2;7;7;7;7m \033[0m");
    n = margin_rects(&s, &t, &g, r, 64);
    assert(at(n, 0, 12 + 2 * 16) == 0x090909);

    /* A selection is not the cell's colour. */
    s.alternate[3 * 10].colour_flags |= XCOLOUR_SELECTED;
    n = margin_rects(&s, &t, &g, r, 64);
    assert(at(n, 0, 12 + 3 * 16) != 0xabcdef);
    s.alternate[3 * 10].colour_flags &= ~XCOLOUR_SELECTED;

    /* Too little room: 0, never a partial set. */
    assert(margin_rects(&s, &t, &g, r, 3) == 0);

    /* Leaving the alternate screen restores the theme background. */
    out("\033[?1049l");
    n = margin_rects(&s, &t, &g, r, 64);
    covers(n, &g);
    assert(n == 4 && at(n, 0, 0) == 0x101010 && at(n, 40, g.window_height - 1) == 0x101010);

    /* A window smaller than the grid (during a resize): nothing outside it. */
    out("\033[?1049h");
    g.window_width = 50; g.window_height = 40;
    n = margin_rects(&s, &t, &g, r, 64);
    { size_t i; for (i = 0; i < n; ++i) assert(r[i].x1 < 50 && r[i].y1 < 40); }

    puts("margin fill PASS: theme background on the normal screen, edge cells on the alternate screen, per row and column, corners, inverse, selection ignored, restore, small window");
    return 0;
}
