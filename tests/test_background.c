#include "../src/background.h"
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* What the opaque path computes for a glyph pixel (cluster_font.c). */
static unsigned opaque(unsigned fg, unsigned bg, unsigned a)
{
    unsigned v = (fg * a + 127) / 255 + (bg * (255 - a) + 127) / 255;
    return v > 255 ? 255 : v;
}

int main(void)
{
    unsigned a, fg, bg, ba;
    unsigned char px[3], al, out[8];
    /* Which path */
    assert(background_path(100, 1, 1) == XBACKGROUND_PATH_OPAQUE);
    assert(background_path(70, 1, 1) == XBACKGROUND_PATH_ALPHA);
    assert(background_path(70, 0, 1) == XBACKGROUND_PATH_OPAQUE);
    assert(background_path(70, 1, 0) == XBACKGROUND_PATH_OPAQUE);
    assert(background_path(70, 0, 0) == XBACKGROUND_PATH_OPAQUE);
    /* Systems whose alpha windows have no frame stay solid. */
    assert(!background_layers_draw_frame(50, 7));
    assert(!background_layers_draw_frame(50, 8));
    assert(background_layers_draw_frame(50, 9));
    assert(background_layers_draw_frame(50, 13));
    assert(background_layers_draw_frame(51, 0));
    assert(!background_layers_draw_frame(45, 30));
    /* Opacity to alpha. The smallest opacity must still take clicks, and
       layers.library takes alpha 0 as "not here". */
    assert(background_alpha_byte(100) == 255);
    assert(background_alpha_byte(50) == 128);
    assert(background_alpha_byte(XBACKGROUND_OPACITY_MIN) == 51);
    assert(background_alpha_byte(XBACKGROUND_OPACITY_MIN) > 0);
    /* Which cells are the default background. */
    {
        XTermCell c;
        memset(&c, 0, sizeof c);
        c.colour_flags = XCOLOUR_DEFAULT_BG | XCOLOUR_DEFAULT_FG;
        assert(background_cell_clear(&c, 0));
        assert(!background_cell_clear(&c, 1));
        c.colour_flags = XCOLOUR_DEFAULT_BG | XCOLOUR_INVERSE;
        assert(!background_cell_clear(&c, 0));
        c.colour_flags = XCOLOUR_RGB_BG;
        assert(!background_cell_clear(&c, 0));
        c.colour_flags = 0; c.bg = 0; /* ANSI black set by a program */
        assert(!background_cell_clear(&c, 0));
        c.colour_flags = XCOLOUR_PEN_BG; c.bg = 0;
        assert(background_cell_clear(&c, 0));
        c.bg = 1;
        assert(!background_cell_clear(&c, 0));
    }
    /* Opaque background: exactly the RGB path, for every value. */
    for (a = 0; a < 256; ++a)
        for (fg = 0; fg < 256; fg += 5)
            for (bg = 0; bg < 256; bg += 5) {
                unsigned p = (fg * a + 127) / 255, alpha = background_over_alpha(a, 255);
                assert(alpha == 255);
                assert(background_over_channel(p, a, bg, 255, alpha) == opaque(fg, bg, a));
            }
    /* Translucent background: the pixel, laid over any backdrop by the
       compositor's straight source-over, shows what the glyph over the
       translucent background should show, within rounding. */
    for (ba = 51; ba < 255; ba += 17)
        for (a = 0; a < 256; a += 3)
            for (fg = 0; fg < 256; fg += 51)
                for (bg = 0; bg < 256; bg += 51) {
                    unsigned alpha = background_over_alpha(a, ba), b;
                    unsigned c = background_over_channel((fg * a + 127) / 255, a, bg, ba, alpha);
                    assert(alpha <= 255 && alpha >= ba);
                    for (b = 0; b < 256; b += 51) {
                        double shown = (c * (double)alpha + b * (255.0 - alpha)) / 255;
                        double want = (fg * (double)a + (bg * (double)ba + b * (255.0 - ba)) * (255.0 - a) / 255) / 255;
                        double d = shown - want;
                        assert(d < 2.5 && d > -2.5);
                    }
                }
    /* A fully covered glyph pixel is solid: sharp text stays solid. */
    assert(background_over_alpha(255, 128) == 255);
    assert(background_over_channel(200, 255, 0, 128, 255) == 200);
    /* Coverage 0: the background itself at its own alpha. */
    assert(background_over_alpha(0, 128) == 128);
    assert(background_over_channel(0, 0, 90, 128, 128) == 90);
    /* An image pixel over the translucent background. */
    px[0] = 10; px[1] = 20; px[2] = 30; al = 128;
    background_over_pixel(px, &al, 200, 100, 50, 255);
    assert(al == 255 && px[0] == 200 && px[1] == 100 && px[2] == 50);
    px[0] = 10; px[1] = 20; px[2] = 30; al = 128;
    background_over_pixel(px, &al, 200, 100, 50, 0);
    assert(al == 128 && px[0] == 10 && px[1] == 20 && px[2] == 30);
    /* Byte order: A, R, G, B, and the pitches are honoured. */
    {
        const unsigned char rgb[] = { 1, 2, 3, 4, 5, 6, 0, 0, 7, 8, 9, 10, 11, 12, 0, 0 };
        const unsigned char alpha[] = { 100, 101, 0, 102, 103, 0 };
        unsigned char argb[16];
        background_interleave(rgb, 8, alpha, 3, 2, 2, argb);
        assert(!memcmp(argb, "\144\1\2\3\145\4\5\6\146\7\10\11\147\12\13\14", 16));
    }
    (void)out;
    puts("background: ok");
    return 0;
}
