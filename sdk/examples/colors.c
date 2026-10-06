/* limpet-colors: the 16 ANSI colours, a 256-colour ramp and a 24-bit
   gradient, each only when the terminal confirmed it.

   Every coloured line also carries its meaning in text, so the output is
   complete without colour: in CON:, in a file, or for a reader who cannot
   tell red from green. Colours are always ended with SGR 39/49 or 0, so the
   Shell prompt that follows is not tinted. */
#include <limpet/limpet.h>
#include <proto/dos.h>
#include <stdio.h>

static const char *names[16] = {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
    "bright black", "bright red", "bright green", "bright yellow",
    "bright blue", "bright magenta", "bright cyan", "bright white"
};

int main(void)
{
    LimpetPresentation p;
    char sgr[LIMPET_SEQUENCE_MAX];
    unsigned i;
    uint32_t level;

    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK) {
        /* Not Limpet: the same information, no escape sequences. */
        puts("Colour test: this console was not identified, so no colours are sent.");
        for (i = 0; i < 16; ++i) printf("  %2u %s\n", i, names[i]);
        return RETURN_OK;
    }
    level = limpet_color_level(&p);
    printf("Colour level reported: %lu (0 none, 1 = 16, 2 = 256, 3 = 24-bit)\n",
           (unsigned long)level);
    if (limpet_uses_pens(&p))
        puts("SGR 30-37 are Workbench pens here; these colours are sent as 38;5;n.");

    /* Name in the default colour, then a text sample and a swatch in the
       colour itself: colour 0 on a dark theme (or 15 on a light one) is
       invisible as text, and the name must stay readable. */
    for (i = 0; i < 16; ++i) {
        printf("  %2u %-15s", i, names[i]);
        if (limpet_sgr_ansi(sgr, sizeof sgr, &p, i, 0))
            printf("%sSample%s", sgr, LIMPET_SGR_FG_DEFAULT);
        if (limpet_sgr_ansi(sgr, sizeof sgr, &p, i, 1))
            printf("  [%s    %s]", sgr, LIMPET_SGR_BG_DEFAULT);
        putchar('\n');
    }

    if (level >= XTY_COLOR_INDEXED256) {
        fputs("256-colour cube, red axis: ", stdout);
        for (i = 0; i < 6; ++i) printf("\033[48;5;%um  ", 16 + 36 * i);
        printf("%s\n", LIMPET_SGR_BG_DEFAULT);
    } else {
        puts("256 colours: not reported, skipped.");
    }

    if (level == XTY_COLOR_RGB) {
        fputs("24-bit gradient:            ", stdout);
        for (i = 0; i < 32; ++i) {
            limpet_sgr_rgb(sgr, sizeof sgr, &p, i * 8, 64, 255 - i * 8, 1);
            printf("%s ", sgr);
        }
        printf("%s\n", LIMPET_SGR_BG_DEFAULT);
    } else {
        puts("24-bit colour: not reported, skipped.");
    }

    if (limpet_has_bold(&p))
        printf("%sBold text%s, then reset with SGR 0.\n", LIMPET_SGR_BOLD, LIMPET_SGR_RESET);
    return RETURN_OK;
}
