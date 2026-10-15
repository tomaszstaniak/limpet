#ifndef AROS_XTERM_FONT_H
#define AROS_XTERM_FONT_H
#include <stdint.h>
struct RastPort;
#define XFONT_CACHE_SIZE 256
#define XFONT_GLYPH_LIMIT 32
enum XFontSource { XFONT_VERA, XFONT_OPEN_SANS, XFONT_NOTO_EMOJI, XFONT_UNIFONT };
enum XFontStatus { XFONT_OK, XFONT_MISSING, XFONT_LOAD_ERROR, XFONT_FORMAT_ERROR,
                   XFONT_SIZE_ERROR, XFONT_CLOSED };
typedef struct XFontGlyph {
    uint32_t scalar, pixels[XFONT_GLYPH_LIMIT];
    int left, top, pitch, advance, error;
    unsigned width, height, index, source, pixel_mode, status, valid;
} XFontGlyph;
/* Single UI-thread owner. Native FreeType faces, fixed grid, bounded cache. */
int xfont_open(void);
/* The same with another main (monospace) face, NULL for Vera Mono, and the
 * pixel size of the main face (13 by default); for a later xfont_open*. */
int xfont_open_main(const char *primary);
void xfont_set_size(unsigned pixels);
/* Paths in XFontSource order. The first two faces are required. */
int xfont_open_paths(const char *const paths[4]);
int xfont_open_extended(const char *const paths[5]);
/* Internal raster backend access; faces live until xfont_close/reopen. */
void *xfont_face(unsigned source);
unsigned xfont_face_baseline(unsigned source);
unsigned xfont_generation(void);
/* Diagnostic view of the actual raster cache; invalidated by a colliding lookup. */
const XFontGlyph *xfont_glyph(uint32_t);
void xfont_close(void);
#ifdef XTERM_FONTS_IN_MEMORY
#include <stddef.h>
/* Bytes of font files held for memory faces (XTERM_FONTS_IN_MEMORY builds). */
size_t xfont_resident_bytes(void);
#endif
unsigned xfont_width(void);
unsigned xfont_height(void);
/* Draw foreground pixels only; caller clears the cell and selects its colour. */
void xfont_draw(struct RastPort *, uint32_t, unsigned, int, int);
#endif
