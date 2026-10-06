/* con-compare: send the same bytes to whatever console runs it, without
   asking first, so that CON: and Limpet can be compared on screen. Each
   line names what it sends and what a terminal that supports it shows.
   A test tool for the documentation's comparison table, not an example:
   real programs must ask first (see sdk/examples). */
#include <stdio.h>

int main(void)
{
    puts("1 UTF-8:      za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x98\x80"
         "   (expect: zazolc with accents, two CJK, one emoji)");
    puts("2 SGR 31:     \033[31mred or pen 1\033[0m");
    puts("3 SGR 38;5:   \033[38;5;196mindexed 196, bright red\033[0m");
    puts("4 SGR 38;2:   \033[38;2;255;128;0m24-bit orange\033[0m");
    puts("5 OSC 8:      \033]8;;https://www.aros.org/\033\\label only\033]8;;\033\\   (expect: 'label only')");
    puts("6 CR + CSI K: XXXXXXXXXXXXXXXXXXXX\rshort line, no X after this\033[K");
    puts("7 Plain URL:  https://www.aros.org/");
    return 0;
}
