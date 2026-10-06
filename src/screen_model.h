#ifndef AROS_XTERM_SCREEN_MODEL_H
#define AROS_XTERM_SCREEN_MODEL_H
#include <stddef.h>
#include <stdint.h>
#include "hyperlink.h"
/* Every synchronous model response fits in this many bytes. */
#define XSCREEN_REPLY_CAPACITY 64
enum { XCOLOUR_DEFAULT_FG=1, XCOLOUR_DEFAULT_BG=2, XCOLOUR_SELECTED=4,
       XCOLOUR_RGB_FG=8, XCOLOUR_RGB_BG=16, XCOLOUR_INVERSE=32,
       XCOLOUR_UNDERLINE=64,
       /* the fg (bg) index 0..7 is an Amiga console pen, not an ANSI colour;
          set only by SGR 30-37 (40-47) in pen mode, cleared by any other fg (bg) */
       XCOLOUR_PEN_FG=128, XCOLOUR_PEN_BG=256 };

typedef struct XTermCell {
    uint32_t ch;
    unsigned char fg, bg, bold, width; /* 1 normal, 2 lead, 0 continuation */
    uint32_t suffix[15];
    unsigned char length; /* 0 blank/continuation, 1..16 cluster scalars */
    unsigned char wrap; /* row-tail only: 1 soft wrap, 2 soft wrap with padding */
    unsigned short colour_flags; /* Default colours are not ANSI white/black. */
    uint32_t link_id; /* Zero plain; nonzero explicit token or invalid sentinel. */
    uint32_t fg_rgb, bg_rgb; /* 0xRRGGBB, active only when the matching flag is set. */
} XTermCell;

typedef struct XTermScreen {
    XTermCell *cells, *history;
    size_t capacity, history_capacity;
    unsigned rows, columns, cursor_row, cursor_column;
    unsigned scroll_top, scroll_bottom; /* Inclusive, zero-based. */
    unsigned history_lines, history_count, history_head, view_offset;
    XTermCell style;
    unsigned params[16], param_count;
    unsigned parser, discard, wrap_pending;
    uint32_t utf8_value;
    unsigned utf8_remaining, utf8_low, utf8_high;
    size_t cluster_cell;
    uint32_t cluster_previous;
    int32_t cluster_state;
    unsigned cluster_active;
    XTermCell *alternate;
    /* Second history-sized buffer: with it, width changes reflow instead of
     * cropping (screen_attach_reflow). Swapped with history on each reflow. */
    XTermCell *reflow_spare;
    unsigned alternate_active, buffer_generation, cursor_visible;
    /* AROS console sessions: SGR 30-37/40-47 select screen pens, as in CON: */
    unsigned amiga_pens;
    unsigned saved_row, saved_column, saved_wrap;
    unsigned saved_scroll_top, saved_scroll_bottom;
    unsigned saved_history_count, saved_history_head, saved_view_offset;
    XTermCell saved_style;
    unsigned csi_private, param_started;
    unsigned csi_intermediate;
    /* Latched at CSI entry. The transport clears this on input-owner change
     * so a fragmented old query cannot answer a new reader. */
    unsigned csi_reply_enabled,escape_reply_enabled;
    unsigned mouse_tracking, mouse_sgr, mouse_generation;
    unsigned bracketed_paste;   /* DEC 2004: the client wants pastes framed */
    unsigned resize_report;     /* DEC 2048: the client wants size changes in band */
    /* Optional synchronous reply sink; must copy bytes and not reenter this
     * model. The transport decides whether a client may receive replies. */
    void (*reply)(void *, const unsigned char *, size_t);
    void *reply_context;
    /* Optional APC sink (Kitty graphics). With none, APC is discarded as
     * before. Payload bytes never reach the grid; runs arrive in order
     * between BEGIN and END or ABORT. Set after screen_init. */
    void (*apc)(void *, int event, const unsigned char *, size_t);
    void *apc_context;
    /* Full-screen upward scrolls so far, for anchoring images to text. */
    unsigned long scroll_count;
    /* Bytes that may change what is shown. APC payload and discarded
     * control strings do not count, so a frontend can skip redrawing while
     * an image is still arriving. Only differences are meaningful. */
    unsigned long visible_bytes;
    XHyperlinks *links;
    uint32_t active_link, saved_link;
    char osc_target[XHYPERLINK_LIMIT];
    unsigned osc_phase, osc_length, osc_invalid, osc_params;
} XTermScreen;

/* Caller owns both storage arrays. Coordinates are zero-based.
 * Resize keeps the top-left rectangle, clamps the cursor, resets scrolling
 * margins to the full screen, and resets history on width changes, unless
 * reflow storage is attached (see screen_attach_reflow).
 * Counts are cells, not bytes.
 * LF preserves the column. UTF-8 scalars wrap on the next glyph.
 * Wide scalars occupy a width=2 lead and a width=0 blank continuation.
 * Erasing/overwriting either half removes the entire pair. Explicit cursor
 * motion counts columns and may land on either half. Extended graphemes use
 * Unicode 17 boundaries and at most 16 inline scalars. Overflow emits U+FFFD
 * in a fresh cell and resets attachment; controls and resize break attachment.
 * Incomplete UTF-8 survives writes; invalid subsequences emit U+FFFD.
 * CSI H/f, A/B/C/D/E/F/G/`/d, J/K, IL/DL (L/M), DECSTBM (r) and
 * basic/indexed/RGB SGR,
 * including inverse video (7/27) and single underline (4/24). DECSTBM uses
 * one-based inclusive rows; LF/automatic wrap scroll within valid margins.
 * Partial-region scrolling does not enter primary scrollback. Alternate
 * screen entry starts with full margins and exit restores primary margins.
 * IL/DL affect cursor-through-bottom-margin rows only, start in column one,
 * and do not append to scrollback; outside the margins they have no effect.
 * Colon-form SGR is not implemented.
 * OSC 8 attaches links.
 * Optional CSI ?1049 h/l snapshots/restores the primary screen, cursor/style
 * and history. Resize crops both screens. Alternate output has no history.
 * CSI ?25 h/l controls cursor visibility. An optional synchronous reply sink
 * handles CSI 18 t size, CSI 5 n status and CSI [ ? ] 6 n cursor queries.
 * Coordinates are live, one-based physical cells (no origin mode yet).
 * Native CSI 0 space q is
 * not a size query in VT mode: it conflicts with DECSCUSR cursor styling.
 */
enum { XSCREEN_APC_BEGIN, XSCREEN_APC_DATA, XSCREEN_APC_END, XSCREEN_APC_ABORT };
int screen_init(XTermScreen *, XTermCell *, size_t, unsigned, unsigned);
/* Buttons 0..2, wheel 64/65, button-motion 32..34 in mode 1002;
 * coordinates zero-based; modifiers VT bits 4/8/16.
 * Returns zero for disabled/invalid/unrepresentable events or a small buffer. */
size_t screen_mouse_encode(const XTermScreen *, unsigned, unsigned, unsigned,
                           unsigned, int, unsigned char *, size_t);
unsigned screen_scalar_width(uint32_t);
int screen_attach_history(XTermScreen *, XTermCell *, size_t);
/* Optional disjoint caller-owned snapshot storage, at least capacity cells. */
int screen_attach_alternate(XTermScreen *, XTermCell *, size_t);
/* Optional disjoint caller-owned storage of exactly the history's size,
 * attached after the history. With it, a width change on the primary screen
 * rejoins soft-wrapped rows (history and screen) and wraps them again at the
 * new width; text, attributes, links and the cursor's place in the text are
 * kept. The alternate screen is still cropped. Without it, resize crops. */
int screen_attach_reflow(XTermScreen *, XTermCell *, size_t);
int screen_resize(XTermScreen *, unsigned, unsigned);
void screen_write(XTermScreen *, const unsigned char *, size_t);
/* Leading bytes that are APC payload: they can produce no reply and may be
 * written in one call even where replies are fed one byte at a time. */
size_t screen_bulk_length(const XTermScreen *, const unsigned char *, size_t);
uint32_t screen_cell(const XTermScreen *, unsigned, unsigned);
/* Positive delta moves into history; negative returns towards live output. */
void screen_scroll_view(XTermScreen *, int);
XTermCell screen_view_cell(const XTermScreen *, unsigned, unsigned);
/* Logical rows: oldest retained history first, then live rows. */
XTermCell screen_line_cell(const XTermScreen *, unsigned, unsigned);
#endif
