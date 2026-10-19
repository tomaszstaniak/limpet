/* Letters keep their place in the cell at every size: the x-height letters
 * share the baseline and top of 'x', and wide ones (w, m) span the cell.
 * Vera Mono's w and W are a pixel wider than the cell at 12 and 14 px; they
 * were shrunk and centred like emoji, and looked raised with a gap before.
 *     test_cluster_letters MAIN_FONT */
#include "../src/aros_font.h"
#include "../src/cluster_font.h"
#include <assert.h>
#include <stdio.h>

typedef struct { int top, bottom, left, right; } Ink;

static Ink ink(uint32_t ch)
{
    const XClusterRaster *r = xfont_cluster(&ch, 1, 1);
    Ink k = {1000, -1, 1000, -1};
    unsigned x, y;
    assert(r->status == XFONT_OK);
    for (y = 0; y < r->height; ++y)
        for (x = 0; x < r->width; ++x)
            if ((r->pixels[y * r->width + x] >> 24) >= 96) {
                if ((int)y < k.top) k.top = y;
                if ((int)y > k.bottom) k.bottom = y;
                if ((int)x < k.left) k.left = x;
                if ((int)x > k.right) k.right = x;
            }
    return k;
}

int main(int argc, char **argv)
{
    const char *paths[5] = {0};
    unsigned size;
    assert(argc == 2);
    paths[0] = paths[1] = argv[1];
    for (size = 10; size <= 18; ++size) {
        const char *letters = "acemnorsuvwxz";
        Ink x;
        xfont_set_size(size);
        assert(xfont_open_extended(paths) == 0);
        x = ink('x');
        for (; *letters; ++letters) {
            Ink k = ink((uint32_t)*letters);
            if (k.bottom < x.bottom - 1 || k.bottom > x.bottom + 1 || k.top < x.top - 1 || k.top > x.top + 1) {
                printf("%u px: '%c' ink rows %d..%d, 'x' %d..%d\n", size, *letters, k.top, k.bottom, x.top, x.bottom);
                return 1;
            }
        }
        {
            /* The same baseline as x exactly: the shrunk w ended a row higher. */
            Ink w = ink('w');
            if (w.bottom != x.bottom) {
                printf("%u px: 'w' ends on row %d, 'x' on row %d\n", size, w.bottom, x.bottom);
                return 1;
            }
            if (w.right - w.left + 2 < (int)xfont_width()) {
                printf("%u px: 'w' spans %d..%d of a %u-pixel cell\n", size, w.left, w.right, xfont_width());
                return 1;
            }
        }
        xfont_close();
    }
    puts("cluster letters PASS: x-height letters on the baseline of x, w spans the cell, 10-18 px");
    return 0;
}
