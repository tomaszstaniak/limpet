#ifndef XTERM_BACKGROUND_H
#define XTERM_BACKGROUND_H
/* The translucent background: the arithmetic and the choice of drawing path,
 * without any system call, so that they can be tested on a host.
 *
 * Only the terminal's default background gets the chosen opacity. Text,
 * cursor, selection, underlines, images and backgrounds a program set
 * itself stay as solid as they are. The compositor takes straight (not
 * premultiplied) alpha and lays it over what is behind (gfx.hidd), so a
 * pixel goes out as colour + alpha, not as a colour already mixed with the
 * terminal's background: an antialiased glyph edge must keep its coverage,
 * or the edge would show a dark or light fringe over the desktop. */
#include "screen_model.h"
#include <stdint.h>
#include <string.h>

#define XBACKGROUND_OPACITY_MIN 20u
#define XBACKGROUND_OPACITY_MAX 100u

enum { XBACKGROUND_PATH_OPAQUE, XBACKGROUND_PATH_ALPHA };

/* 100 % takes the old RGB path; so does a screen or window without an alpha
 * surface, whatever the setting says. */
static inline int background_path(unsigned percent, int screen_supports, int window_composited)
{
    return percent < XBACKGROUND_OPACITY_MAX && screen_supports && window_composited
        ? XBACKGROUND_PATH_ALPHA : XBACKGROUND_PATH_OPAQUE;
}

static inline unsigned char background_alpha_byte(unsigned percent)
{
    if (percent >= 100) return 255;
    return (unsigned char)((percent * 255 + 50) / 100);
}

/* Which cells have the terminal's default background: the default colour,
 * or pen 0, which is that colour under its Amiga name (the Shell's prompt
 * leaves it behind on every line). Everything else (selection, inverse, a
 * colour set by the program, even one equal to the theme's) keeps a solid
 * background. */
static inline int background_cell_clear(const XTermCell *cell, int selected)
{
    return !selected && !(cell->colour_flags & XCOLOUR_INVERSE) &&
           ((cell->colour_flags & XCOLOUR_DEFAULT_BG) ||
            ((cell->colour_flags & XCOLOUR_PEN_BG) && !cell->bg));
}

/* One colour channel and the alpha of premultiplied `premultiplied` at
 * coverage `a` over a background of straight `bg` at alpha `bg_alpha`, as
 * straight alpha. bg_alpha 255 gives exactly the opaque path's colour. */
static inline unsigned background_over_alpha(unsigned a, unsigned bg_alpha)
{
    return a + (bg_alpha * (255 - a) + 127) / 255;
}
static inline unsigned char background_over_channel(unsigned premultiplied, unsigned a, unsigned bg,
                                                    unsigned bg_alpha, unsigned alpha)
{
    unsigned v = premultiplied + (bg * bg_alpha * (255 - a) + 32512) / 65025;
    if (!alpha) return 0;
    v = (v * 255 + alpha / 2) / alpha;
    return (unsigned char)(v > 255 ? 255 : v);
}

/* A straight colour at alpha a (an image's RGBA pixel) laid over a pixel of
 * the frame, which is straight colour + alpha itself. */
static inline void background_over_pixel(unsigned char *rgb, unsigned char *alpha,
                                         unsigned r, unsigned g, unsigned b, unsigned a)
{
    unsigned under = *alpha, out = background_over_alpha(a, under);
    rgb[0] = background_over_channel((r * a + 127) / 255, a, rgb[0], under, out);
    rgb[1] = background_over_channel((g * a + 127) / 255, a, rgb[1], under, out);
    rgb[2] = background_over_channel((b * a + 127) / 255, a, rgb[2], under, out);
    *alpha = (unsigned char)out;
}

/* The bytes WritePixelArray() takes for RECTFMT_ARGB, whatever the host's
 * byte order: A, R, G, B. */
static inline void background_interleave(const unsigned char *rgb, unsigned rgb_pitch,
                                         const unsigned char *alpha, unsigned alpha_pitch,
                                         unsigned width, unsigned height, unsigned char *argb)
{
    unsigned x, y;
    for (y = 0; y < height; ++y) {
        const unsigned char *c = rgb + (size_t)y * rgb_pitch, *a = alpha + (size_t)y * alpha_pitch;
        for (x = 0; x < width; ++x) {
            *argb++ = a[x]; *argb++ = c[0]; *argb++ = c[1]; *argb++ = c[2];
            c += 3;
        }
    }
}
#endif
