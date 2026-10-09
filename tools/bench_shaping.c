/* Shaping benchmark for AROS: the clusters of tests/shaping_corpus.h, split
 * by the screen model as the terminal splits them, through xfont_cluster.
 *
 *   bench-shaping OUTFILE [rounds]
 *
 * Writes the corpus result to OUTFILE in the format of the host test, then
 * times three loops of `rounds` each (default 20): opening the fonts alone;
 * opening them and shaping every cluster (a fresh font generation, so every
 * cluster is shaped, as on its first appearance on screen); and shaping
 * every cluster again with the cache warm. Cold shaping time is the second
 * loop minus the first. Ticks are DateStamp ticks of the guest clock, which
 * under TCG need not run at real time: compare builds within one boot,
 * alternating them, not across boots. CPU benchmark only; no window. */
#include "../src/aros_font.h"
#include "../src/cluster_font.h"
#include "../src/screen_model.h"
#include "../tests/shaping_corpus.h"
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CELLS 512
static struct { uint32_t s[XCLUSTER_MAX]; unsigned count, span, line, col; } cells[MAX_CELLS];
static unsigned ncells;
static XTermScreen screen;
static XTermCell screen_cells[2 * 40];
static const char *const paths[5] = {"FONTS:TrueType/VeraMono.ttf",
    "FONTS:TrueType/OpenSans-Regular.ttf", "PROGDIR:Fonts/NotoEmoji.ttf",
    "PROGDIR:Fonts/Unifont.otf", "PROGDIR:Fonts/NotoColorEmoji.ttf"};

static unsigned long ticks(void)
{
    struct DateStamp t;
    DateStamp(&t);
    return ((unsigned long)t.ds_Days * 24 * 60 + t.ds_Minute) * 60 * TICKS_PER_SECOND + t.ds_Tick;
}

static void collect(void)
{
    unsigned k, col, i;
    for (k = 0; k < sizeof corpus / sizeof corpus[0]; ++k) {
        if (screen_init(&screen, screen_cells, sizeof screen_cells / sizeof screen_cells[0], 2, 40)) exit(20);
        screen_write(&screen, (const unsigned char *)corpus[k].text, strlen(corpus[k].text));
        for (col = 0; col < 40 && ncells < MAX_CELLS; ++col) {
            XTermCell c = screen_view_cell(&screen, 0, col);
            unsigned count;
            if (!c.width || (!c.length && c.ch == ' ')) continue;
            count = c.length ? c.length : 1;
            if (count > XCLUSTER_MAX) continue;
            cells[ncells].s[0] = c.ch;
            for (i = 1; i < count; ++i) cells[ncells].s[i] = c.suffix[i - 1];
            cells[ncells].count = count; cells[ncells].span = c.width;
            cells[ncells].line = k; cells[ncells].col = col; ++ncells;
        }
    }
}

static uint32_t hash(const XClusterRaster *r)
{
    uint32_t h = 2166136261u;
    unsigned i, b;
    for (i = 0; i < r->width * r->height; ++i)
        for (b = 0; b < 4; ++b) { h ^= (r->pixels[i] >> (8 * b)) & 255; h *= 16777619u; }
    return h;
}

static uint32_t shape_all(void)
{
    uint32_t sum = 0;
    unsigned i;
    for (i = 0; i < ncells; ++i) sum += xfont_cluster(cells[i].s, cells[i].count, cells[i].span)->glyphs;
    return sum;
}

int main(int argc, char **argv)
{
    unsigned rounds = argc > 2 ? (unsigned)atoi(argv[2]) : 20, r, i;
    unsigned long t0, open_t, cold_t, warm_t;
    uint32_t check = 0;
    FILE *out;
    if (argc < 2 || !rounds) { puts("usage: bench-shaping OUTFILE [rounds]"); return 10; }
    collect();
    if (xfont_open_extended(paths)) return 20;
    if (!(out = fopen(argv[1], "w"))) return 20;
    for (i = 0; i < ncells; ++i) {
        const XClusterRaster *x = xfont_cluster(cells[i].s, cells[i].count, cells[i].span);
        fprintf(out, "%s col=%u U+%04X len=%u span=%u source=%u glyphs=%u status=%u colour=%u %ux%u pixels=%08x\n",
                corpus[cells[i].line].name, cells[i].col, (unsigned)cells[i].s[0], cells[i].count,
                cells[i].span, x->source, x->glyphs, x->status, x->colour, x->width, x->height,
                (unsigned)hash(x));
    }
    fclose(out);
    t0 = ticks();
    for (r = 0; r < rounds; ++r) if (xfont_open_extended(paths)) return 20;
    open_t = ticks() - t0;
    t0 = ticks();
    for (r = 0; r < rounds; ++r) { if (xfont_open_extended(paths)) return 20; check += shape_all(); }
    cold_t = ticks() - t0;
    t0 = ticks();
    for (r = 0; r < rounds; ++r) check += shape_all();
    warm_t = ticks() - t0;
    printf("bench-shaping: %u clusters, %u rounds, ticks at %u Hz: open %lu cold %lu warm %lu "
           "(cold shaping = cold - open), check %lu\n", ncells, rounds, (unsigned)TICKS_PER_SECOND,
           open_t, cold_t, warm_t, (unsigned long)check);
    xfont_close();
    return 0;
}
