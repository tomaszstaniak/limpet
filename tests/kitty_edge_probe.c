/* Guest probe: an inline image placed at the bottom edge of the window.

   kitty_edge_probe place C0|C1 HEIGHT_PX ID [ROW]
       Moves the cursor to ROW (1-based; default: the last row) by printing
       lines from the top of a cleared screen, draws a 64 x HEIGHT_PX RGB
       image with the given cursor policy, then prints one line of text.
       C=0 must leave the whole image visible and the text below it
       (scrolling as needed); C=1 must leave the cursor where it was.
   kitty_edge_probe delete ID
       Deletes the placement and frees the image.

   Writes nothing but plain text unless the presentation query answered:
   CON: would show the base64 payload as text. Built against the Limpet
   SDK library (sdk/lib/<ABI>/libLimpet.a). */
#include <limpet/limpet.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64
#define MAX_HEIGHT 1024

static unsigned char pixels[WIDTH * MAX_HEIGHT * 3];

static int out(const char *s)
{ return limpet_write_dos((void *)Output(), s, strlen(s)); }

int main(int argc, char **argv)
{
    LimpetPresentation p;
    LimpetImage image;
    unsigned height, row, rows, y, x;
    char line[96];
    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK) {
        printf("not Limpet: nothing drawn\n");
        return 5;
    }
    if (argc == 3 && !strcmp(argv[1], "delete")) {
        int r = limpet_kitty_delete((uint32_t)strtoul(argv[2], NULL, 10), 1,
                                    limpet_write_dos, (void *)Output());
        return r ? 10 : 0;
    }
    if ((argc != 5 && argc != 6) || strcmp(argv[1], "place") ||
        (strcmp(argv[2], "C0") && strcmp(argv[2], "C1"))) {
        printf("usage: kitty_edge_probe place C0|C1 HEIGHT_PX ID [ROW] | delete ID\n");
        return 10;
    }
    height = (unsigned)strtoul(argv[3], NULL, 10);
    if (!height || height > MAX_HEIGHT) { printf("height 1..%u\n", MAX_HEIGHT); return 10; }
    rows = limpet_rows(&p);
    if (!rows) { printf("rows unknown\n"); return 10; }
    row = argc == 6 ? (unsigned)strtoul(argv[5], NULL, 10) : rows;
    if (!row || row > rows) row = rows;
    for (y = 0; y < height; ++y)
        for (x = 0; x < WIDTH; ++x) {
            unsigned char *px = pixels + (y * WIDTH + x) * 3;
            /* Horizontal bands every 8 pixels, so a cut-off bottom shows. */
            px[0] = (unsigned char)((y / 8) & 1 ? 230 : 40);
            px[1] = (unsigned char)(y * 255 / height);
            px[2] = (unsigned char)(x * 4);
        }
    /* Clear the screen and stop on row `row`, column 1. */
    out("\033[2J\033[H");
    for (y = 1; y < row; ++y) {
        snprintf(line, sizeof line, "line %u of %u\r\n", y, rows);
        if (out(line)) return 10;
    }
    snprintf(line, sizeof line, "img %s>", argv[2]);
    out(line);
    memset(&image, 0, sizeof image);
    image.id = (uint32_t)strtoul(argv[4], NULL, 10);
    image.width = WIDTH;
    image.height = height;
    image.format = 24;
    image.keep_cursor = argv[2][1] == '1';
    image.quiet = 2;
    if (limpet_kitty_transmit(&image, pixels, limpet_write_dos, (void *)Output()))
        return 10;
    snprintf(line, sizeof line, "<text after the image (%s)\r\n", argv[2]);
    out(line);
    return 0;
}
