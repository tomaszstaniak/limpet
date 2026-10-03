#ifndef AROS_XTERM_KITTY_GRAPHICS_H
#define AROS_XTERM_KITTY_GRAPHICS_H
/* A documented subset of the Kitty terminal graphics protocol
   (https://sw.kovidgoyal.net/kitty/graphics-protocol/), fed by the screen
   model's APC sink. Portable C with no AROS dependencies, host-testable.

   Supported:
     a=t transmit, a=T transmit and display, a=p display, a=d delete, a=q query
     f=24 RGB, f=32 RGBA (alpha blended onto the text beneath)
     t=d direct transmission, base64 payload, chunked with m=1 ... m=0
     i image id 1..4294967295 (required), one placement per image;
       displaying again moves it; placement ids (p) are not supported
     retransmitting an id replaces its pixels; a placement keeps showing it
     d=a/A all placements (A also frees images), d=i/I by id (I also frees)
     q=0/1/2 reply suppression, C=0/1 cursor movement after display
   Everything else (PNG, files, shared memory, compression, animation
   frames, image numbers, extra placements, other delete targets, unicode
   placeholders, cropping and scaling keys) gets ENOTSUPPORTED.

   A new image replaces the old one only when its last chunk has arrived and
   the byte count is exact; an interrupted or malformed transmission is
   dropped and frees its buffer, leaving the previous image intact. Replies
   follow Kitty: none without an image id, OK suppressed by q=1, errors by
   q=2. A string shaped like a reply (only i, I and p keys) is ignored
   silently: in a cooked-mode tty the terminal's own reply is echoed back to
   it, and answering the echo would start a reply loop. */
#include <stddef.h>
#include <stdint.h>

#define XKITTY_MAX_IMAGES 16
#define XKITTY_CONTROL_MAX 512
#define XKITTY_REPLY_MAX 160

typedef struct {
    unsigned max_width, max_height;   /* pixels */
    size_t max_image_bytes;           /* decoded pixels of one image */
    size_t max_total_bytes;           /* all images plus one transmission */
} XKittyLimits;

typedef struct {
    uint32_t id;                      /* 0: free slot */
    unsigned width, height, channels; /* channels 3 or 4 */
    unsigned char *pixels;
    size_t bytes;
    uint64_t generation;              /* +1 for every complete transmission */
    uint64_t serial;                  /* unique per commit, also across reused slots */
    int placed;
    long placed_row;                  /* absolute: screen row + scroll count */
    unsigned placed_column, placed_alternate;
} XKittyImage;

typedef struct {
    unsigned long frames_received;    /* complete transmissions committed */
    unsigned long transmissions_dropped; /* interrupted or invalid */
    unsigned long long payload_bytes; /* base64 bytes received */
    unsigned long commands, errors;
    size_t memory_bytes, memory_peak; /* images plus open transmission */
} XKittyStats;

/* Where the cursor is when a command ends, and the cell size in pixels. */
typedef struct {
    unsigned row, column, rows, columns, alternate;
    unsigned long scroll_count;
    unsigned cell_width, cell_height;
} XKittyCursor;

typedef struct {
    int changed;                      /* placements or displayed pixels */
    int move_cursor;
    unsigned cursor_row, cursor_column;
    unsigned char reply[XKITTY_REPLY_MAX];
    size_t reply_length;
} XKittyResult;

typedef struct {
    XKittyLimits limits;
    XKittyImage images[XKITTY_MAX_IMAGES];
    XKittyStats stats;
    uint64_t commits;                 /* source of image serials; never reset */
    /* Current APC string: state, whether it starts with 'G' (-1 unknown). */
    int state, is_kitty;
    char control[XKITTY_CONTROL_MAX];
    size_t control_length;
    /* Open transmission, possibly across several APC strings. */
    int transmitting;
    uint32_t tx_id;
    unsigned tx_width, tx_height, tx_channels, tx_quiet, tx_cursor, tx_display;
    unsigned char *tx_pixels;
    size_t tx_bytes, tx_filled;
    unsigned tx_quad, tx_quad_length, tx_padding, tx_error;
    /* Command parsed from the current string's control data. */
    unsigned cmd_quiet, cmd_more, cmd_cursor;
    char cmd_action, cmd_delete;
    uint32_t cmd_id;
    int cmd_continuation, cmd_echo;
    const char *cmd_error;
} XKitty;

void xkitty_init(XKitty *, const XKittyLimits *);
/* Frees every image and any open transmission. */
void xkitty_reset(XKitty *);
void xkitty_apc_begin(XKitty *);
void xkitty_apc_data(XKitty *, const unsigned char *, size_t);
void xkitty_apc_abort(XKitty *);
/* The string ended with ST. Fills result; returns nonzero if it was Kitty. */
int xkitty_apc_end(XKitty *, const XKittyCursor *, XKittyResult *);
/* First screen row (may be negative) and row count a placement covers. */
int xkitty_placement_rows(const XKittyImage *, const XKittyCursor *, long *first, unsigned *count);
#endif
