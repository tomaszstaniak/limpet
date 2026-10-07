/* limpet-image: one inline image through the Kitty graphics subset.

   Order of checks, cheapest and least intrusive first:
   1. the presentation query on Output(): no answer means not Limpet, and
      no image byte is ever sent (CON: would print the base64 as text);
   2. the Kitty query (a=q), whose reply arrives as input. Replies are
      delivered in raw mode only, so the program switches Input() to raw
      for the exchange. The whole reply must arrive within one second,
      measured from the moment the query is written.
   Then it draws a 160x96 gradient, waits for a key (at most 30 s), deletes
   the image and frees its memory in the terminal.

   Room for the image is made first. Limpet places an image at the cursor
   and clips it at the bottom of the window; it does not scroll the text up
   to fit it, and it does not report its cell size in pixels. So the program
   prints enough blank lines for the image at a cell height of 12 pixels
   (Limpet's default font is taller, so a line or two stay empty), moves
   back up and draws with the cursor kept in place.

   Every exit path after raw mode was entered restores cooked mode, and a
   drawn image is deleted again even when a later step fails. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#define W 160
#define H 96
#define QUERY_ID 7001u
#define IMAGE_ID 7002u
#define REPLY_LIMIT_MS 1000
#define KEY_LIMIT_MS 30000
/* Rows reserved for the image: H divided by the smallest cell height
   assumed, rounded up. */
#define RESERVED_ROWS ((H + 11) / 12)

static unsigned char pixels[W * H * 3];

static long now_ms(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (d.ds_Days * 1440L + d.ds_Minute) * 60000L + d.ds_Tick * (1000L / TICKS_PER_SECOND);
}

/* Read until ESC \, the buffer is full or the deadline passes; one
   deadline for the whole reply, not one per byte. Returns the byte count,
   or -1 when Read() fails. */
static long read_reply(char *reply, size_t size, long deadline)
{
    size_t n = 0;
    char c;
    for (;;) {
        long left = deadline - now_ms();
        if (n + 1 >= size || left <= 0) break;
        if (!WaitForChar(Input(), left * 1000)) break;
        if (Read(Input(), &c, 1) != 1) { reply[n] = 0; return -1; }
        reply[n++] = c;
        if (n >= 2 && reply[n - 2] == '\033' && c == '\\') break;
    }
    reply[n] = 0;
    return (long)n;
}

static int stdio_write(void *context, const char *bytes, size_t length)
{
    (void)context;
    return fwrite(bytes, 1, length, stdout) != length || fflush(stdout) != 0;
}

static int raw(int on)
{
    return SetMode(Input(), on ? 1 : 0) != 0;
}

/* The Kitty query in raw mode. 1 supported, 0 not, -1 the console failed. */
static int kitty_supported(char *message, size_t message_size)
{
    char query[64], reply[128];
    long n;
    int answer;
    limpet_kitty_query(query, sizeof query, QUERY_ID);
    if (!raw(1)) { snprintf(message, message_size, "cannot switch the console to raw mode"); return -1; }
    if (stdio_write(NULL, query, strlen(query))) {
        raw(0);
        snprintf(message, message_size, "writing the query failed");
        return -1;
    }
    n = read_reply(reply, sizeof reply, now_ms() + REPLY_LIMIT_MS);
    if (!raw(0)) { snprintf(message, message_size, "cannot switch the console back to cooked mode"); return -1; }
    if (n < 0) { snprintf(message, message_size, "reading the reply failed"); return -1; }
    answer = limpet_kitty_parse_reply(reply, (size_t)n, QUERY_ID, message, message_size);
    if (answer < 0) snprintf(message, message_size, "no reply within %d ms", REPLY_LIMIT_MS);
    return answer == 1;
}

int main(void)
{
    LimpetPresentation p;
    LimpetImage image = { IMAGE_ID, W, H, 24, 1, 2 };
    char message[64], key;
    unsigned x, y, i;
    int supported, rc = RETURN_OK;

    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK) {
        puts("Not a Limpet console: no image sent.");
        return RETURN_OK;
    }
    if (!IsInteractive(Input())) {
        puts("Input is not the console, so the image query cannot be answered: no image sent.");
        return RETURN_OK;
    }
    supported = kitty_supported(message, sizeof message);
    if (supported != 1) {
        printf("Kitty graphics not available (%s): no image sent.\n", message);
        return supported < 0 ? RETURN_WARN : RETURN_OK;
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
        /* The SDK already ended the partial transmission; whatever arrived
           of the image is removed too. */
        limpet_kitty_delete(IMAGE_ID, 1, stdio_write, NULL);
        for (i = 0; i < RESERVED_ROWS; ++i) putchar('\n');
        puts("Writing the image failed.");
        return RETURN_FAIL;
    }
    for (i = 0; i < RESERVED_ROWS; ++i) putchar('\n');
    fputs("Press a key to remove the image (30 s limit).", stdout);
    fflush(stdout);
    if (raw(1)) {
        if (WaitForChar(Input(), KEY_LIMIT_MS * 1000L) && Read(Input(), &key, 1) != 1) rc = RETURN_WARN;
        if (!raw(0)) rc = RETURN_WARN;
    } else {
        rc = RETURN_WARN;          /* no key wait without raw mode; delete at once */
    }
    /* d=I: remove the placement and free the pixels in the terminal. An
       image left behind counts against Limpet's 16-image limit. */
    if (limpet_kitty_delete(IMAGE_ID, 1, stdio_write, NULL)) rc = RETURN_FAIL;
    puts(rc == RETURN_OK ? "\nImage deleted." : "\nImage deleted; the console reported an error.");
    return rc;
}
