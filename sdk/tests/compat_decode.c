/* Decodes a reply file with whichever SDK it is built against and prints
 * the fields every SDK version has, so two versions can be compared. */
#include <limpet/limpet.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    XtyControl c;
    LimpetPresentation p;
    FILE *f;
    if (argc != 2 || !(f = fopen(argv[1], "rb")) || fread(&c, sizeof c, 1, f) != 1) return 2;
    fclose(f);
    if (limpet_presentation_decode(&c, &p) != LIMPET_OK) { puts("status=invalid"); return 1; }
    /* Older clients consume only bits their headers define. */
    printf("status=ok known&31=%u color=%u styles=%u lines=%u rows=%u columns=%u pens=%u",
           (unsigned)(p.known & 31u), (unsigned)p.color_level, (unsigned)(p.styles & XTY_STYLE_BOLD_RESET), (unsigned)p.lines,
           (unsigned)p.rows, (unsigned)p.columns, (unsigned)limpet_uses_pens(&p));
#ifdef XTY_PRESENT_KNOWN_INPUT
    /* SDK 0.2.0 and later. */
    printf(" input=%u", (unsigned)p.input);
#endif
#ifdef XTY_PRESENT_KNOWN_FEATURES
    /* SDK 0.3.0 and later. */
    printf(" features=%u", (unsigned)(p.features & XTY_FEATURE_TITLE));
#endif
    putchar('\n');
    return 0;
}
