/* limpet-image: one inline image through the Kitty graphics subset.

   Order of checks, cheapest and least intrusive first:
   1. the presentation query on Output(): no answer means not Limpet, and
      no image byte is ever sent (CON: would print the base64 as text);
   2. the Kitty query (a=q), whose reply arrives as input. Replies are
      delivered in raw mode only, so the program switches Input() to raw
      for as long as it waits, with a one second limit, and always back.
   Then it draws a 160x96 gradient, waits for a key (at most 30 s), deletes
   the image and frees its memory in the terminal.

   Room for the image is made first. Limpet 0.1 places an image at the
   cursor and clips it at the bottom of the window; it does not scroll the
   text up to fit it, and it does not report its cell size in pixels. So
   the program prints enough blank lines for the image at a cell height of
   12 pixels (Limpet's default font is taller, so a line or two stay
   empty), moves back up and draws with the cursor kept in place. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#define W 160
#define H 96
#define QUERY_ID 7001u
#define IMAGE_ID 7002u
/* Rows reserved for the image: H divided by the smallest cell height
   assumed, rounded up. */
#define RESERVED_ROWS ((H + 11) / 12)

static unsigned char pixels[W * H * 3];

/* Read until ESC \ or the limit; returns the byte count. Each wait is
   bounded, so a console that never answers costs one second. */
static size_t read_reply(char *reply, size_t size, LONG microseconds)
{
    size_t n = 0;
    char c;
    while (n + 1 < size && WaitForChar(Input(), microseconds) && Read(Input(), &c, 1) == 1) {
        reply[n++] = c;
        if (n >= 2 && reply[n - 2] == '\033' && c == '\\') break;
    }
    reply[n] = 0;
    return n;
}

static int stdio_write(void *context, const char *bytes, size_t length)
{
    (void)context;
    return fwrite(bytes, 1, length, stdout) != length || fflush(stdout) != 0;
}

int main(void)
{
    LimpetPresentation p;
    LimpetImage image = { IMAGE_ID, W, H, 24, 1, 2 };
    char query[64], reply[128], message[64], key;
    unsigned x, y, i;
    int answer;

    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK) {
        puts("Not a Limpet console: no image sent.");
        return RETURN_OK;
    }
    if (!IsInteractive(Input())) {
        puts("Input is not the console, so the image query cannot be answered: no image sent.");
        return RETURN_OK;
    }

    limpet_kitty_query(query, sizeof query, QUERY_ID);
    SetMode(Input(), 1);
    fputs(query, stdout);
    fflush(stdout);
    answer = limpet_kitty_parse_reply(reply, read_reply(reply, sizeof reply, 1000000),
                                      QUERY_ID, message, sizeof message);
    SetMode(Input(), 0);
    if (answer != 1) {
        printf("Kitty graphics not available (%s): no image sent.\n",
               answer == 0 ? message : "no reply within 1 s");
        return RETURN_OK;
    }

    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x) {
            unsigned char *px = pixels + (y * W + x) * 3;
            px[0] = (unsigned char)(x * 255 / (W - 1));
            px[1] = (unsigned char)(y * 255 / (H - 1));
            px[2] = (unsigned char)(((x / 16 + y / 16) & 1) ? 200 : 90);
        }
    puts("A 160x96 RGB image, sent as base64 in 4 KiB chunks:");
    /* Scroll the room into view, return to its top, draw with C=1 (the
       cursor stays), then step below it. q=2: no reply, because none is
       read in cooked mode. */
    for (i = 0; i < RESERVED_ROWS; ++i) putchar('\n');
    printf("\033[%uA", RESERVED_ROWS);
    if (limpet_kitty_transmit(&image, pixels, stdio_write, NULL)) {
        for (i = 0; i < RESERVED_ROWS; ++i) putchar('\n');
        puts("Writing the image failed.");
        return RETURN_FAIL;
    }
    for (i = 0; i < RESERVED_ROWS; ++i) putchar('\n');
    fputs("Press a key to remove the image (30 s limit).", stdout);
    fflush(stdout);
    SetMode(Input(), 1);
    if (WaitForChar(Input(), 30000000)) Read(Input(), &key, 1);
    SetMode(Input(), 0);
    /* d=I: remove the placement and free the pixels in the terminal. An
       image left behind counts against Limpet's 16-image limit. */
    limpet_kitty_delete(IMAGE_ID, 1, stdio_write, NULL);
    puts("\nImage deleted.");
    return RETURN_OK;
}
