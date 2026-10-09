/* Shaping corpus: what the renderer would draw for each cluster of a fixed
 * set of lines, one output line per cell. The lines go through the screen
 * model (clusters and widths as the terminal splits them) and then through
 * xfont_cluster with the cell's width, as aros_xterm.c does. Each result is
 * printed with its font source, glyph count, status, size and a hash of the
 * pixels, so a HarfBuzz or FreeType update shows exactly which clusters
 * changed. tests/shaping-corpus.expected holds the output of the build that
 * was last accepted; refresh it only after looking at every changed line. */
#include "../src/aros_font.h"
#include "../src/cluster_font.h"
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "shaping_corpus.h"

static XTermScreen s;
static XTermCell cells[2 * 40];

static uint32_t hash(const XClusterRaster *r)
{
    uint32_t h = 2166136261u;
    unsigned i, b;
    for (i = 0; i < r->width * r->height; ++i)
        for (b = 0; b < 4; ++b) { h ^= (r->pixels[i] >> (8 * b)) & 255; h *= 16777619u; }
    return h;
}

int main(int argc, char **argv)
{
    const char *paths[5];
    unsigned i, k, col;
    assert(argc == 6);
    for (i = 0; i < 5; ++i) paths[i] = argv[i + 1];
    assert(xfont_open_extended(paths) == 0);
    for (k = 0; k < sizeof corpus / sizeof corpus[0]; ++k) {
        assert(screen_init(&s, cells, sizeof cells / sizeof cells[0], 2, 40) == 0);
        screen_write(&s, (const unsigned char *)corpus[k].text, strlen(corpus[k].text));
        for (col = 0; col < 40; ++col) {
            XTermCell cell = screen_view_cell(&s, 0, col);
            uint32_t scalars[XCLUSTER_MAX];
            const XClusterRaster *r;
            unsigned count;
            if (!cell.width || (!cell.length && cell.ch == ' ')) continue;
            count = cell.length ? cell.length : 1;
            if (count > XCLUSTER_MAX) continue;
            scalars[0] = cell.ch;
            for (i = 1; i < count; ++i) scalars[i] = cell.suffix[i - 1];
            r = xfont_cluster(scalars, count, cell.width);
            printf("%s col=%u U+%04X len=%u span=%u source=%u glyphs=%u status=%u colour=%u %ux%u pixels=%08x\n",
                   corpus[k].name, col, (unsigned)scalars[0], count, cell.width, r->source,
                   r->glyphs, r->status, r->colour, r->width, r->height, (unsigned)hash(r));
        }
    }
    return 0;
}
