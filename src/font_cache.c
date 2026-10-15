#include "aros_font.h"
#ifdef __AROS__
#include <proto/freetype2.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#include <stdio.h>
#include <string.h>
#ifdef XTERM_FONTS_IN_MEMORY
#include <stdlib.h>
#endif

static FT_Library library;
static FT_Face faces[5];
static unsigned face_baselines[5], generation;
static unsigned cell_width, cell_height, baseline;
static XFontGlyph cache[XFONT_CACHE_SIZE];
#ifdef XTERM_FONTS_IN_MEMORY
/* Whole font files, owned until FT_Done_Face: FreeType reads a memory face
 * from this buffer for its whole life. The ABIv11 DOS handler worker needs
 * this because a file-based FT_Stream does DOS Seek/Read in the calling
 * process on every uncached glyph and every HarfBuzz table fetch, and that
 * process's port also receives its clients' packets. */
static unsigned char *face_data[5];
static size_t face_size[5]; /* set only once FreeType accepted the face */

size_t xfont_resident_bytes(void)
{
    size_t total = 0; unsigned i;
    for (i = 0; i < 5; ++i) total += face_size[i];
    return total;
}

static FT_Error open_memory_face(const char *path, unsigned i)
{
    FILE *file = fopen(path, "rb");
    long size;
    if (!file) return FT_Err_Cannot_Open_Resource;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) ||
        !(face_data[i] = malloc((size_t)size)) ||
        fread(face_data[i], 1, (size_t)size, file) != (size_t)size) {
        fclose(file); free(face_data[i]); face_data[i] = NULL;
        return FT_Err_Cannot_Open_Stream;
    }
    fclose(file);
    {
        FT_Error error = FT_New_Memory_Face(library, face_data[i], (FT_Long)size, 0, &faces[i]);
        if (!error) face_size[i] = (size_t)size;
        return error;
    }
}

static void drop_face(unsigned i)
{
    if (faces[i]) FT_Done_Face(faces[i]);
    faces[i] = NULL;
    if (face_data[i]) {
        free(face_data[i]); face_data[i] = NULL;
    }
    face_size[i] = 0;
}
#endif

void *xfont_face(unsigned source) { return source < 5 ? faces[source] : NULL; }
unsigned xfont_face_baseline(unsigned source) { return source < 5 ? face_baselines[source] : 0; }
unsigned xfont_generation(void) { return generation; }

void xfont_close(void)
{
    unsigned i;
    ++generation;
#ifdef XTERM_FONTS_IN_MEMORY
    for (i = 0; i < 5; ++i) drop_face(i);
#else
    for (i = 0; i < 5; ++i) {
        if (faces[i]) FT_Done_Face(faces[i]);
        faces[i] = NULL;
    }
#endif
    if (library) FT_Done_FreeType(library);
    library = NULL;
    cell_width = cell_height = baseline = 0;
    memset(cache, 0, sizeof cache);
}

int xfont_open_paths(const char *const paths[4])
{
    const char *extended[5]; unsigned i;
    if (!paths) { xfont_close(); return -1; }
    for(i=0;i<4;++i) extended[i]=paths[i];
    extended[4]=NULL;
    return xfont_open_extended(extended);
}

static unsigned primary_size = 13;
void xfont_set_size(unsigned pixels) { primary_size = pixels >= 2 ? pixels : 13; }

int xfont_open_extended(const char *const paths[5])
{
    unsigned i;
    /* At the default 13 px these are the sizes Limpet always used:
       Unifont's 16 px grid scales with the main font. */
    const unsigned sizes[5] = {primary_size, primary_size, primary_size - 1,
                               (primary_size * 16 + 6) / 13, 16};
    xfont_close();
    if (!paths || FT_Init_FreeType(&library)) return -1;
    for (i = 0; i < 5; ++i) {
        FT_Error error = 0;
        if (paths[i]) {
#ifdef XTERM_FONTS_IN_MEMORY
            error = open_memory_face(paths[i], i);
#else
            error = FT_New_Face(library, paths[i], 0, &faces[i]);
#endif
            if (!error) error = FT_Select_Charmap(faces[i], FT_ENCODING_UNICODE);
            if (!error) {
                if (i==4 && faces[i]->num_fixed_sizes>0)
                    error=FT_Select_Size(faces[i],0);
                else error = FT_Set_Pixel_Sizes(faces[i], 0, sizes[i]);
            }
        }
        if (!paths[i] || error) {
#ifdef XTERM_FONTS_IN_MEMORY
            drop_face(i);
#else
            if (faces[i]) FT_Done_Face(faces[i]);
            faces[i] = NULL;
#endif
            if (i < 2) {
                printf("Required font %u unavailable (error=%d)\n", i, error);
                xfont_close(); return -1;
            }
            /* A missing optional face is normal (none ships NotoColorEmoji)
               and is not reported: every start printed this, and on mainline
               each stdio line to a console, or the 4 KB stdout buffer when
               redirected, stays allocated after exit
               (docs/release-resources-20260929.md). */
        }
    }
    cell_width = (unsigned)((faces[0]->size->metrics.max_advance + 63) / 64);
    baseline = (unsigned)((faces[0]->size->metrics.ascender + 63) / 64);
    cell_height = baseline + (unsigned)((-faces[0]->size->metrics.descender + 63) / 64);
    if (!FT_IS_FIXED_WIDTH(faces[0]) || !cell_width || !cell_height ||
        cell_width > XFONT_GLYPH_LIMIT || cell_height > XFONT_GLYPH_LIMIT) {
        xfont_close(); return -1;
    }
    for (i = 0; i < 5; ++i) face_baselines[i] = baseline;
    /* Unifont's 16px grid has a 14px ascent, one more than 13px Vera Mono.
     * Keep its full 16 rows inside our 17px cell instead of clipping row 0. */
    if (faces[XFONT_UNIFONT])
        face_baselines[XFONT_UNIFONT] = (unsigned)((faces[XFONT_UNIFONT]->size->metrics.ascender + 63) / 64);
    return 0;
}

unsigned xfont_width(void) { return cell_width; }
unsigned xfont_height(void) { return cell_height; }

const XFontGlyph *xfont_glyph(uint32_t scalar)
{
    XFontGlyph *g = &cache[scalar % XFONT_CACHE_SIZE];
    FT_Face face;
    FT_Bitmap *bitmap;
    unsigned row, col, source;
    if (g->valid && g->scalar == scalar) return g;
    memset(g, 0, sizeof *g); g->scalar = scalar; g->valid = 1;
    if (!library) { g->status = XFONT_CLOSED; return g; }
    for (source = 0; source < 4; ++source) {
        if (!faces[source]) continue;
        g->index = FT_Get_Char_Index(faces[source], scalar);
        if (g->index) break;
    }
    if (!g->index) { source = 1; g->status = XFONT_MISSING; }
    g->source = source; face = faces[source];
    g->error = FT_Load_Glyph(face, g->index, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME);
    if (g->error) { g->status = XFONT_LOAD_ERROR; return g; }
    bitmap = &face->glyph->bitmap;
    g->pixel_mode = bitmap->pixel_mode; g->pitch = bitmap->pitch;
    g->width = bitmap->width; g->height = bitmap->rows;
    g->left = face->glyph->bitmap_left;
    g->top = (int)face_baselines[source] - face->glyph->bitmap_top;
    g->advance = (int)face->glyph->advance.x;
    if (bitmap->pixel_mode != FT_PIXEL_MODE_MONO) { g->status = XFONT_FORMAT_ERROR; return g; }
    if (bitmap->width > XFONT_GLYPH_LIMIT || bitmap->rows > XFONT_GLYPH_LIMIT) {
        g->status = XFONT_SIZE_ERROR; return g;
    }
    for (row = 0; row < g->height; ++row) {
        const unsigned char *source_row = bitmap->pitch >= 0 ?
            bitmap->buffer + row * bitmap->pitch :
            bitmap->buffer + (g->height - row - 1) * -bitmap->pitch;
        for (col = 0; col < g->width; ++col)
            if (source_row[col / 8] & (0x80 >> (col % 8)))
                g->pixels[row] |= (uint32_t)1 << col;
    }
    return g;
}
