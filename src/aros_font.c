#include "aros_font.h"
#include <proto/graphics.h>
#include <stdio.h>

int xfont_open(void) { return xfont_open_main(NULL); }

int xfont_open_main(const char *primary)
{
    const char *paths[5] = {
        "FONTS:TrueType/VeraMono.ttf",
        "FONTS:TrueType/OpenSans-Regular.ttf",
        "PROGDIR:Fonts/NotoEmoji.ttf",
        "PROGDIR:Fonts/Unifont.otf",
        "PROGDIR:Fonts/NotoColorEmoji.ttf"
    };
    if (primary) paths[0] = primary;
    /* No start-up banner on stdout: see the note on optional faces in
       font_cache.c. Errors are still printed. */
    return xfont_open_extended(paths) ? -1 : 0;
}

void xfont_draw(struct RastPort *rp, uint32_t scalar, unsigned span, int x, int y)
{
    const XFontGlyph *g;
    unsigned row, col;
    if (scalar == ' ' || span < 1 || span > 2) return;
    g = xfont_glyph(scalar);
    if (g->status != XFONT_OK && g->status != XFONT_MISSING) return;
    for (row = 0; row < g->height; ++row) {
        int dy = g->top + (int)row;
        if (dy < 0 || dy >= (int)xfont_height()) continue;
        for (col = 0; col < g->width; ++col) {
            int dx = g->left + (int)col;
            if (dx >= 0 && dx < (int)(xfont_width() * span) &&
                (g->pixels[row] & ((uint32_t)1 << col))) WritePixel(rp, x + dx, y + dy);
        }
    }
}
