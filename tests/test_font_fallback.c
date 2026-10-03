#include "../src/aros_font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(uint32_t scalar, unsigned source, unsigned span)
{
    const XFontGlyph *g = xfont_glyph(scalar);
    unsigned i, ink = 0;
    printf("U+%04X source=%u index=%u status=%u error=%d mode=%u bitmap=%ux%u pitch=%d xy=%d,%d advance=%d\n",
           (unsigned)scalar, g->source, g->index, g->status, g->error, g->pixel_mode,
           g->width, g->height, g->pitch, g->left, g->top, g->advance);
    fflush(stdout);
    assert(g->status == XFONT_OK && g->index != 0);
    assert(g->source == source && g->pixel_mode == 1);
    assert(g->left >= 0 && g->top >= 0);
    assert(g->left + g->width <= xfont_width() * span);
    assert(g->top + g->height <= xfont_height());
    for (i = 0; i < g->height; ++i) ink |= g->pixels[i];
    assert(ink != 0);
}

#ifdef XTERM_FONTS_IN_MEMORY
static long file_bytes(const char *path)
{
    FILE *f = fopen(path, "rb"); long n;
    assert(f && !fseek(f, 0, SEEK_END));
    n = ftell(f); fclose(f); return n;
}
#endif

int main(int argc, char **argv)
{
    const char *paths[4];
    XFontGlyph saved, reloaded;
    unsigned i;
    if (argc != 5) { puts("Usage: test-font vera open-sans noto-emoji unifont"); return 20; }
    for (i = 0; i < 4; ++i) paths[i] = argv[i + 1];
    assert(xfont_open_paths(paths) == 0);
    assert(xfont_width() == 8 && xfont_height() == 17);
    check('A', XFONT_VERA, 1);
    check(0x105, XFONT_OPEN_SANS, 1);
    check(0x4e2d, XFONT_UNIFONT, 2);
    check(0x6587, XFONT_UNIFONT, 2);
    check(0xd55c, XFONT_UNIFONT, 2);
    check(0xae00, XFONT_UNIFONT, 2);
    check(0x1f600, XFONT_NOTO_EMOJI, 2);
    check(0x1f680, XFONT_NOTO_EMOJI, 2);
    check(0x1f980, XFONT_NOTO_EMOJI, 2);
    saved = *xfont_glyph(0x4e2d);
    (void)xfont_glyph(0x4e2d + XFONT_CACHE_SIZE);
    reloaded = *xfont_glyph(0x4e2d);
    assert(saved.index == reloaded.index && saved.source == reloaded.source);
    assert(memcmp(saved.pixels, reloaded.pixels, sizeof saved.pixels) == 0);
    assert(xfont_glyph(0x10ffff)->status == XFONT_MISSING);
    xfont_close();
    assert(xfont_glyph('A')->status == XFONT_CLOSED);
    paths[2] = paths[3] = NULL;
    assert(xfont_open_paths(paths) == 0);
    check('A', XFONT_VERA, 1);
    assert(xfont_glyph(0x4e2d)->status == XFONT_MISSING);
    xfont_close();
#ifdef XTERM_FONTS_IN_MEMORY
    /* Resident bytes count only faces FreeType accepted: a rejected optional
     * face (here the test executable, which is no font) must not add to it. */
    {
        long vera = file_bytes(argv[1]), sans = file_bytes(argv[2]);
        paths[2] = argv[0];
        assert(xfont_open_paths(paths) == 0);
        assert(xfont_resident_bytes() == (size_t)(vera + sans));
        xfont_close();
        assert(xfont_resident_bytes() == 0);
        paths[2] = NULL;
    }
#endif
    paths[0] = NULL;
    assert(xfont_open_paths(paths) != 0);
    xfont_close();
    puts("font fallback tests passed (coverage, MONO raster, bounds, cache, optional fonts, cleanup)");
    return 0;
}
