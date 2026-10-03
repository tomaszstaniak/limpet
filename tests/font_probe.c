/* Native/host FreeType probe. No GUI, no font installation, no shaping claims. */
#ifdef __AROS__
#include <proto/freetype2.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FT_Library library;
    FT_Face face;
    FT_Error error;
    FT_Int major, minor, patch;
    unsigned i, failures = 0;
    const FT_ULong chars[] = {0x4e2d, 0x6587, 0xd55c, 0xae00, 0x1f600, 0x1f680, 0x1f980};
    if (argc != 3) { puts("Usage: font-probe font-file pixel-size"); return 20; }
    error = FT_Init_FreeType(&library);
    if (error) return 20;
    FT_Library_Version(library, &major, &minor, &patch);
    printf("FreeType %d.%d.%d font=%s size=%s\n", major, minor, patch, argv[1], argv[2]);
    error = FT_New_Face(library, argv[1], 0, &face);
    if (error) { printf("open error=%d\n", error); FT_Done_FreeType(library); return 20; }
    printf("family=%s glyphs=%ld scalable=%d fixed=%d strikes=%d\n",
           face->family_name, face->num_glyphs, !!FT_IS_SCALABLE(face),
           !!FT_IS_FIXED_WIDTH(face), face->num_fixed_sizes);
    error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (!error) error = FT_Set_Pixel_Sizes(face, 0, (unsigned)atoi(argv[2]));
    printf("size error=%d asc=%ld desc=%ld height=%ld advance=%ld\n", error,
           face->size->metrics.ascender, face->size->metrics.descender,
           face->size->metrics.height, face->size->metrics.max_advance);
    if (!error) for (i = 0; i < sizeof chars / sizeof chars[0]; ++i) {
        FT_UInt index = FT_Get_Char_Index(face, chars[i]);
        if (!index) { printf("U+%04lX MISSING\n", chars[i]); continue; }
        error = FT_Load_Glyph(face, index, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME);
        if (error) { printf("U+%04lX index=%u load error=%d\n", chars[i], index, error); ++failures; continue; }
        printf("U+%04lX index=%u mode=%u bitmap=%ux%u pitch=%d left=%d top=%d advance=%ld\n",
               chars[i], index, face->glyph->bitmap.pixel_mode, face->glyph->bitmap.width,
               face->glyph->bitmap.rows, face->glyph->bitmap.pitch,
               face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->advance.x);
    }
    FT_Done_Face(face); FT_Done_FreeType(library);
    return failures || error ? 20 : 0;
}
