#include "screen_model.h"
#include <string.h>
#include <stdio.h>
#include "unicode_width_table.h"
#include "utf8proc.h"

enum { GROUND, ESCAPE, CSI, OSC, OSC_ESCAPE, STRING_DISCARD, STRING_ESCAPE, APC, APC_ESCAPE };
static const XTermCell blank = { ' ', 7, 0, 0, 1, {0}, 0, 0,
    XCOLOUR_DEFAULT_FG | XCOLOUR_DEFAULT_BG, 0, 0, 0 };

unsigned screen_scalar_width(uint32_t scalar)
{
    size_t low = 0, high = sizeof wide_ranges / sizeof wide_ranges[0];
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (scalar < wide_ranges[middle].first) high = middle;
        else if (scalar > wide_ranges[middle].last) low = middle + 1;
        else return 2;
    }
    return 1;
}

static void fill(XTermCell *cells, size_t count, XTermCell style)
{
    size_t i;
    style.ch = ' ';
    style.width = 1;
    style.length = 0;
    style.wrap = 0;
    style.link_id = 0;
    memset(style.suffix, 0, sizeof style.suffix);
    for (i = 0; i < count; ++i) cells[i] = style;
}

/* The half-open interval may intersect either end of a two-cell glyph.
 * Extend to both ends before blanking; valid pairs never cross a row. */
static void erase_range(XTermScreen *s, size_t first, size_t end)
{
    size_t total = (size_t)s->rows * s->columns;
    if (first >= end || end > total) return;
    if (s->cells[first].width == 0 && first % s->columns) --first;
    if (s->cells[end - 1].width == 2 && end < total && end % s->columns) ++end;
    fill(s->cells + first, end - first, s->style);
}

int screen_init(XTermScreen *s, XTermCell *cells, size_t count,
                unsigned rows, unsigned columns)
{
    if (!s || !cells || !rows || !columns || rows > count / columns)
        return -1;
    memset(s, 0, sizeof *s);
    s->cells = cells; s->capacity = count;
    s->rows = rows; s->columns = columns; s->style = blank;
    s->scroll_bottom = rows - 1;
    s->cursor_visible = 1;
    fill(cells, (size_t)rows * columns, blank);
    return 0;
}

int screen_attach_history(XTermScreen *s, XTermCell *cells, size_t count)
{
    if (!s || s->alternate_active || !cells || count < s->columns ||
        count / s->columns > 65535) return -1;
    s->history = cells; s->history_capacity = count;
    s->history_lines = (unsigned)(count / s->columns);
    s->history_count = s->history_head = s->view_offset = 0;
    return 0;
}

int screen_attach_alternate(XTermScreen *s, XTermCell *cells, size_t count)
{
    if(!s || !s->cells || !cells || cells==s->cells || cells==s->history ||
       count<s->capacity || s->alternate_active) return -1;
    s->alternate=cells;
    return 0;
}

int screen_attach_reflow(XTermScreen *s, XTermCell *cells, size_t count)
{
    if (!s || !s->history || !cells || count != s->history_capacity ||
        cells == s->cells || cells == s->history || cells == s->alternate) return -1;
    s->reflow_spare = cells;
    return 0;
}

static void alternate_mode(XTermScreen *s,unsigned enabled)
{
    size_t bytes=(size_t)s->rows*s->columns*sizeof *s->cells;
    if(!s->alternate || s->alternate_active==enabled) return;
    if(enabled) {
        memcpy(s->alternate,s->cells,bytes);
        s->saved_row=s->cursor_row; s->saved_column=s->cursor_column;
        s->saved_wrap=s->wrap_pending; s->saved_style=s->style;
        s->saved_scroll_top=s->scroll_top; s->saved_scroll_bottom=s->scroll_bottom;
        s->saved_link=s->active_link; s->active_link=0;
        s->saved_history_count=s->history_count;
        s->saved_history_head=s->history_head; s->saved_view_offset=s->view_offset;
        s->history_count=s->history_head=s->view_offset=0;
        fill(s->cells,(size_t)s->rows*s->columns,s->style);
        s->cursor_row=s->cursor_column=s->wrap_pending=0;
        s->scroll_top=0; s->scroll_bottom=s->rows-1;
    } else {
        memcpy(s->cells,s->alternate,bytes);
        s->cursor_row=s->saved_row; s->cursor_column=s->saved_column;
        s->wrap_pending=s->saved_wrap; s->style=s->saved_style;
        s->scroll_top=s->saved_scroll_top; s->scroll_bottom=s->saved_scroll_bottom;
        s->active_link=s->saved_link;
        s->history_count=s->saved_history_count;
        s->history_head=s->saved_history_head; s->view_offset=s->saved_view_offset;
    }
    s->alternate_active=enabled; ++s->buffer_generation;
    s->cluster_active=0; s->cluster_state=0;
}

/* Append one screen row to the primary scrollback ring. A scrolled-back
 * view keeps showing the same content, as for a line feed. */
static void history_push(XTermScreen *s, const XTermCell *row)
{
    memcpy(s->history + (size_t)s->history_head * s->columns, row,
           s->columns * sizeof *s->cells);
    s->history_head = (s->history_head + 1) % s->history_lines;
    if (s->history_count < s->history_lines) ++s->history_count;
    if (s->view_offset && s->view_offset < s->history_count) ++s->view_offset;
}


/* Reflow writer: output rows go to a ring in the spare buffer. */
typedef struct {
    XTermCell *ring;
    unsigned columns, slots;      /* ring holds `slots` rows */
    unsigned long rows;           /* rows started so far */
    unsigned column;              /* next column in the current row */
    long last_content, cursor_row;
    unsigned cursor_column;
} Reflow;

static XTermCell *reflow_row(Reflow *w, unsigned long row)
{
    return w->ring + (size_t)(row % w->slots) * w->columns;
}

static void reflow_begin(Reflow *w)
{
    fill(reflow_row(w, w->rows), w->columns, blank);
    ++w->rows; w->column = 0;
}

/* Continue the logical line on a new row: a full row gets a soft wrap. */
static void reflow_break(Reflow *w, unsigned char kind)
{
    reflow_row(w, w->rows - 1)[w->columns - 1].wrap = kind;
    reflow_begin(w);
}

static void reflow_put(Reflow *w, const XTermCell *cell, const XTermCell *next)
{
    XTermCell *row;
    if (w->column == w->columns) reflow_break(w, 1);
    /* A wide glyph that does not fit leaves a padding cell, as on output. */
    if (cell->width == 2 && w->column + 1 >= w->columns && w->columns > 1)
        reflow_break(w, 2);
    row = reflow_row(w, w->rows - 1);
    row[w->column] = *cell; row[w->column].wrap = 0;
    if (cell->width == 2) {
        if (w->columns > 1 && next) {
            row[w->column + 1] = *next; row[w->column + 1].wrap = 0;
            ++w->column;
        } else row[w->column].width = 1;
    }
    ++w->column;
    w->last_content = (long)w->rows - 1;
}

static void reflow_mark_cursor(Reflow *w)
{
    if (w->column == w->columns) reflow_break(w, 1);
    w->cursor_row = (long)w->rows - 1; w->cursor_column = w->column;
}

/* Cells no text was written to, as erase and fill leave them. The erase
 * colour does not count: AROS One's prompt erases with pen 0, and keeping
 * those cells would wrap an invisible tail onto an extra row. */
static int untouched(const XTermCell *c)
{
    return c->ch == ' ' && !c->length && !c->link_id &&
        !(c->colour_flags & (XCOLOUR_INVERSE | XCOLOUR_UNDERLINE));
}

/* One source row of a logical line. `cursor` is the cursor's column in this
 * row, or -1. Returns nonzero when the logical line continues. */
static int reflow_source_row(Reflow *w, const XTermCell *row, unsigned columns, int cursor)
{
    unsigned char wrap = row[columns - 1].wrap;
    unsigned used = wrap == 2 ? columns - 1 : columns, c;
    if (!wrap) while (used && untouched(&row[used - 1])) --used;
    if (cursor >= 0 && (unsigned)cursor > used) used = (unsigned)cursor;
    for (c = 0; c < used; ++c) {
        if ((int)c == cursor) reflow_mark_cursor(w);
        if (row[c].width == 0) continue;   /* emitted with its lead */
        reflow_put(w, &row[c], c + 1 < columns ? &row[c + 1] : NULL);
    }
    if ((int)used == cursor) reflow_mark_cursor(w);
    return wrap != 0;
}

/* Rejoin and rewrap the primary screen and its history at a new width. */
static void reflow(XTermScreen *s, unsigned rows, unsigned columns)
{
    Reflow w;
    unsigned r, live_end = 0, old_lines = s->history_lines;
    unsigned long start, first_kept;
    int continues = 0;
    XTermCell *swap;
    memset(&w, 0, sizeof w);
    w.ring = s->reflow_spare; w.columns = columns;
    w.slots = (unsigned)(s->history_capacity / columns);
    w.last_content = w.cursor_row = -1;
    /* Rows below the last text and the cursor are empty; leave them out. */
    for (r = 0; r < s->rows; ++r) {
        const XTermCell *row = s->cells + (size_t)r * s->columns;
        unsigned c;
        for (c = 0; c < s->columns; ++c)
            if (!untouched(&row[c])) { live_end = r + 1; break; }
    }
    if (live_end < s->cursor_row + 1) live_end = s->cursor_row + 1;
    for (r = 0; r < s->history_count; ++r) {
        unsigned line = (s->history_head + old_lines - s->history_count + r) % old_lines;
        if (!continues) reflow_begin(&w);
        continues = reflow_source_row(&w, s->history + (size_t)line * s->columns,
                                      s->columns, -1);
    }
    for (r = 0; r < live_end; ++r) {
        if (!continues) reflow_begin(&w);
        continues = reflow_source_row(&w, s->cells + (size_t)r * s->columns, s->columns,
                                      r == s->cursor_row ?
                                      (int)(s->cursor_column + (s->wrap_pending ? 1 : 0)) : -1);
    }
    if (!w.rows) reflow_begin(&w);
    if (w.cursor_row < 0) { w.cursor_row = (long)w.rows - 1; w.cursor_column = 0; }
    if (w.last_content < w.cursor_row) w.last_content = w.cursor_row;
    /* The screen ends with the last text; the cursor stays on it. */
    start = (unsigned long)w.last_content + 1 > rows ? (unsigned long)w.last_content + 1 - rows : 0;
    if ((unsigned long)w.cursor_row < start) start = (unsigned long)w.cursor_row;
    for (r = 0; r < rows; ++r) {
        XTermCell *dst = s->cells + (size_t)r * columns;
        if (start + r < w.rows) memcpy(dst, reflow_row(&w, start + r), columns * sizeof *dst);
        else fill(dst, columns, blank);
    }
    /* What remains before `start` in the ring is the new history. */
    first_kept = w.rows > w.slots ? w.rows - w.slots : 0;
    swap = s->history; s->history = s->reflow_spare; s->reflow_spare = swap;
    s->history_lines = w.slots;
    s->history_count = start > first_kept ? (unsigned)(start - first_kept) : 0;
    s->history_head = (unsigned)(start % w.slots);
    s->view_offset = 0;
    s->scroll_count += start;
    s->cursor_row = (unsigned)((unsigned long)w.cursor_row - start);
    s->cursor_column = w.cursor_column < columns ? w.cursor_column : columns - 1;
}

int screen_resize(XTermScreen *s, unsigned rows, unsigned columns)
{
    unsigned r, common_rows, common_columns;
    int width_changed;
    if (!s || !rows || !columns || rows > s->capacity / columns) return -1;
    if(s->alternate_active) {
        /* Reuse the exact crop/wide-pair rules for the saved primary screen.
         * The temporary has no alternate, so this recurses at most once. */
        XTermScreen primary=*s;
        primary.cells=s->alternate; primary.alternate=NULL; primary.alternate_active=0;
        primary.cursor_row=s->saved_row; primary.cursor_column=s->saved_column;
        primary.wrap_pending=s->saved_wrap;
        primary.history_count=s->saved_history_count;
        primary.history_head=s->saved_history_head; primary.view_offset=s->saved_view_offset;
        screen_resize(&primary,rows,columns);
        s->saved_row=primary.cursor_row; s->saved_column=primary.cursor_column;
        s->saved_wrap=0;
        s->saved_scroll_top=0; s->saved_scroll_bottom=rows-1;
        s->saved_history_count=primary.history_count;
        s->saved_history_head=primary.history_head; s->saved_view_offset=primary.view_offset;
        s->history=primary.history; s->reflow_spare=primary.reflow_spare;
        s->history_lines=primary.history_lines;
    }
    width_changed=columns!=s->columns;
    if (width_changed && !s->alternate_active && s->history && s->reflow_spare) {
        reflow(s, rows, columns);
        s->rows = rows; s->columns = columns;
        s->scroll_top = 0; s->scroll_bottom = rows - 1;
        s->wrap_pending = 0; s->cluster_active = 0; s->cluster_state = 0;
        return 0;
    }
    /* Shrinking keeps the cursor row (normally the prompt) on screen, as
     * xterm does: drop rows from the top instead of cropping the bottom.
     * They enter scrollback only on the primary screen and only when the
     * width is unchanged; a width change clears history below (no reflow). */
    if (rows < s->rows && s->cursor_row >= rows) {
        unsigned shift = s->cursor_row - rows + 1;
        if (!width_changed && !s->alternate_active && s->history && s->history_lines)
            for (r = 0; r < shift; ++r) history_push(s, s->cells + (size_t)r * s->columns);
        memmove(s->cells, s->cells + (size_t)shift * s->columns,
                (size_t)(s->rows - shift) * s->columns * sizeof *s->cells);
        s->rows -= shift;
        s->cursor_row -= shift;
        /* The text moved up like a scroll; placed images follow it. */
        s->scroll_count += shift;
    }
    common_rows = rows < s->rows ? rows : s->rows;
    common_columns = columns < s->columns ? columns : s->columns;
    /* Move backwards when the destination stride grows, forwards otherwise. */
    for (r = 0; r < common_rows; ++r) {
        unsigned y = columns > s->columns ? common_rows - r - 1 : r;
        memmove(s->cells + (size_t)y * columns,
                s->cells + (size_t)y * s->columns,
                common_columns * sizeof *s->cells);
        fill(s->cells + (size_t)y * columns + common_columns,
             columns - common_columns, blank);
        if (s->cells[(size_t)y * columns + columns - 1].width == 2) {
            XTermCell *edge = s->cells + (size_t)y * columns + columns - 1;
            fill(edge, 1, *edge);
        }
    }
    fill(s->cells + (size_t)common_rows * columns,
         (size_t)(rows - common_rows) * columns, blank);
    if (columns != s->columns) {
        s->history_lines = (unsigned)(s->history_capacity / columns);
        s->history_head = s->history_count = s->view_offset = 0;
    }
    s->rows = rows; s->columns = columns;
    s->scroll_top = 0; s->scroll_bottom = rows - 1;
    /* Resize has no reflow: old wrap links no longer describe this layout. */
    if (width_changed)
        for (r = 0; r < rows * columns; ++r) s->cells[r].wrap = 0;
    if (s->cursor_row >= rows) s->cursor_row = rows - 1;
    if (s->cursor_column >= columns) s->cursor_column = columns - 1;
    s->wrap_pending = 0;
    s->cluster_active = 0;
    s->cluster_state = 0;
    return 0;
}

static void line_feed(XTermScreen *s)
{
    unsigned top, bottom;
    if (s->cursor_row != s->scroll_bottom) {
        if (s->cursor_row + 1 < s->rows) ++s->cursor_row;
        return;
    }
    top = s->scroll_top; bottom = s->scroll_bottom;
    if (top == 0 && bottom == s->rows - 1 &&
        !s->alternate_active && s->history && s->history_lines)
        history_push(s, s->cells);
    memmove(s->cells + (size_t)top * s->columns,
            s->cells + (size_t)(top + 1) * s->columns,
            (size_t)(bottom - top) * s->columns * sizeof *s->cells);
    fill(s->cells + (size_t)bottom * s->columns, s->columns, s->style);
    if (top == 0 && bottom == s->rows - 1) ++s->scroll_count;
}

static void scroll_region_rows(XTermScreen *s, unsigned count, int down)
{
    unsigned top = s->scroll_top, bottom = s->scroll_bottom;
    unsigned height = bottom - top + 1;
    size_t columns = s->columns;
    if (count > height) count = height;
    if (down) {
        if (count < height)
            memmove(s->cells + (size_t)(top + count) * columns,
                    s->cells + (size_t)top * columns,
                    (size_t)(height - count) * columns * sizeof *s->cells);
        fill(s->cells + (size_t)top * columns,
             (size_t)count * columns, s->style);
    } else {
        if (count < height)
            memmove(s->cells + (size_t)top * columns,
                    s->cells + (size_t)(top + count) * columns,
                    (size_t)(height - count) * columns * sizeof *s->cells);
        fill(s->cells + (size_t)(bottom - count + 1) * columns,
             (size_t)count * columns, s->style);
        if (count < height)
            s->cells[(size_t)(bottom - count + 1) * columns - 1].wrap = 0;
    }
    if (top) s->cells[(size_t)top * columns - 1].wrap = 0;
    s->cells[(size_t)(bottom + 1) * columns - 1].wrap = 0;
    if (!down && top == 0 && bottom == s->rows - 1) s->scroll_count += count;
}

size_t screen_mouse_encode(const XTermScreen *s, unsigned button, unsigned col,
                           unsigned row, unsigned modifiers, int release,
                           unsigned char *out, size_t capacity)
{
    unsigned code;
    unsigned char bytes[64];
    size_t n;
    if(!s || !s->mouse_tracking || !out || col>=s->columns || row>=s->rows ||
       (button>2 && button!=64 && button!=65 &&
        !(s->mouse_tracking==1002 && button>=32 && button<=34 && !release)) || (modifiers&~28u) ||
       (release && button>=64)) return 0;
    code=button|modifiers;
    if(s->mouse_sgr) {
        int length=snprintf((char *)bytes,sizeof bytes,"\033[<%u;%u;%u%c",
                            code,col+1,row+1,release?'m':'M');
        if(length<=0 || (size_t)length>=sizeof bytes) return 0;
        n=(size_t)length;
    } else {
        if(col>=223 || row>=223) return 0;
        bytes[0]=27; bytes[1]='['; bytes[2]='M';
        bytes[3]=(unsigned char)(32+(release?(3|modifiers):code));
        bytes[4]=(unsigned char)(33+col); bytes[5]=(unsigned char)(33+row);
        n=6;
    }
    if(capacity<n) return 0;
    memcpy(out,bytes,n);
    return n;
}

size_t screen_wheel_encode(const XTermScreen *s, int up, unsigned col, unsigned row,
                           unsigned modifiers, unsigned char *out, size_t capacity)
{
    char bytes[48];
    int length;
    if(!s || !s->wheel_report || !out || col>=s->columns || row>=s->rows || (modifiers&~28u)) return 0;
    length=snprintf(bytes,sizeof bytes,"\033[<%u;%u;%uM",(up?64u:65u)|modifiers,col+1,row+1);
    if(length<=0 || (size_t)length>capacity) return 0;
    memcpy(out,bytes,(size_t)length);
    return (size_t)length;
}

static unsigned parameter(const XTermScreen *s, unsigned index, unsigned fallback)
{
    return index < s->param_count && s->params[index] ? s->params[index] : fallback;
}

static void execute_csi(XTermScreen *s, unsigned char command)
{
    unsigned i, n = parameter(s, 0, 1);
    unsigned mode = s->params[0];
    size_t cursor = (size_t)s->cursor_row * s->columns + s->cursor_column;
    size_t total = (size_t)s->rows * s->columns;
    if (s->discard) return;
    /* Queries observe the live cursor, including a pending right-margin wrap.
     * Never fall through to the cursor-motion wrap reset below. */
    if(command=='n' && !s->csi_intermediate && s->param_count==1) {
        if(s->reply && s->csi_reply_enabled) {
            if(mode==5 && !s->csi_private)
                s->reply(s->reply_context,(const unsigned char *)"\033[0n",4);
            else if(mode==6) {
                char response[XSCREEN_REPLY_CAPACITY];
                int length=snprintf(response,sizeof response,"\033[%s%u;%uR",
                    s->csi_private?"?":"",s->cursor_row+1,s->cursor_column+1);
                if(length>0 && (size_t)length<sizeof response)
                    s->reply(s->reply_context,(const unsigned char *)response,(size_t)length);
            }
        }
        return;
    }
    if (!s->csi_private && s->param_count==1 &&
        command=='t' && !s->csi_intermediate && mode==18) {
        if(s->reply && s->csi_reply_enabled) {
            char response[XSCREEN_REPLY_CAPACITY];
            int length=snprintf(response,sizeof response,
                "\033[8;%u;%ut",s->rows,s->columns);
            if(length>0 && (size_t)length<sizeof response)
                s->reply(s->reply_context,(const unsigned char *)response,(size_t)length);
        }
        return;
    }
    if(s->csi_intermediate) return;
    if(s->csi_private) {
        if(command=='h' || command=='l') {
            for(i=0;i<s->param_count;++i) {
                if(s->params[i]==1049) alternate_mode(s,command=='h');
                else if(s->params[i]==25) s->cursor_visible=command=='h';
                else if(s->params[i]==1000 || s->params[i]==1002) {
                    unsigned tracking=command=='h'?s->params[i]:0;
                    if(tracking!=s->mouse_tracking) ++s->mouse_generation;
                    s->mouse_tracking=tracking;
                } else if(s->params[i]==1006) {
                    unsigned sgr=command=='h';
                    if(sgr!=s->mouse_sgr) ++s->mouse_generation;
                    s->mouse_sgr=sgr;
                }
                else if(s->params[i]==2004) s->bracketed_paste=command=='h';
                else if(s->params[i]==2048) s->resize_report=command=='h';
                else if(s->params[i]==2100) s->wheel_report=command=='h';
            }
        }
        return;
    }
    switch (command) {
    case 'S': scroll_region_rows(s, n, 0); break;
    case 'T': scroll_region_rows(s, n, 1); break;
    case 'L': {
        unsigned available;
        if (s->cursor_row < s->scroll_top || s->cursor_row > s->scroll_bottom)
            return;
        available = s->scroll_bottom - s->cursor_row + 1;
        if (n > available) n = available;
        if (n < available)
            memmove(s->cells + (size_t)(s->cursor_row + n) * s->columns,
                    s->cells + (size_t)s->cursor_row * s->columns,
                    (size_t)(available - n) * s->columns * sizeof *s->cells);
        fill(s->cells + (size_t)s->cursor_row * s->columns,
             (size_t)n * s->columns, s->style);
        if (s->cursor_row)
            s->cells[(size_t)s->cursor_row * s->columns - 1].wrap = 0;
        s->cells[(size_t)(s->scroll_bottom + 1) * s->columns - 1].wrap = 0;
        s->cursor_column = 0;
        break;
    }
    case 'M': {
        unsigned available;
        if (s->cursor_row < s->scroll_top || s->cursor_row > s->scroll_bottom)
            return;
        available = s->scroll_bottom - s->cursor_row + 1;
        if (n > available) n = available;
        if (n < available) {
            memmove(s->cells + (size_t)s->cursor_row * s->columns,
                    s->cells + (size_t)(s->cursor_row + n) * s->columns,
                    (size_t)(available - n) * s->columns * sizeof *s->cells);
            s->cells[(size_t)(s->scroll_bottom - n + 1) * s->columns - 1].wrap = 0;
        }
        fill(s->cells + (size_t)(s->scroll_bottom - n + 1) * s->columns,
             (size_t)n * s->columns, s->style);
        if (s->cursor_row)
            s->cells[(size_t)s->cursor_row * s->columns - 1].wrap = 0;
        s->cursor_column = 0;
        break;
    }
    case 'r': {
        unsigned top = parameter(s, 0, 1);
        unsigned bottom = parameter(s, 1, s->rows);
        if (s->param_count > 2 || top >= bottom || bottom > s->rows) return;
        s->scroll_top = top - 1; s->scroll_bottom = bottom - 1;
        s->cursor_row = s->cursor_column = 0;
        break;
    }
    case 'H': case 'f':
        s->cursor_row = parameter(s, 0, 1) - 1;
        s->cursor_column = parameter(s, 1, 1) - 1;
        if (s->cursor_row >= s->rows) s->cursor_row = s->rows - 1;
        if (s->cursor_column >= s->columns) s->cursor_column = s->columns - 1;
        break;
    case 'A': s->cursor_row -= n < s->cursor_row ? n : s->cursor_row; break;
    case 'B':
        s->cursor_row += n < s->rows - 1 - s->cursor_row ? n : s->rows - 1 - s->cursor_row;
        break;
    case 'C':
        s->cursor_column += n < s->columns - 1 - s->cursor_column ? n : s->columns - 1 - s->cursor_column;
        break;
    case 'D': s->cursor_column -= n < s->cursor_column ? n : s->cursor_column; break;
    case 'E':
        s->cursor_row += n < s->rows-1-s->cursor_row ? n : s->rows-1-s->cursor_row;
        s->cursor_column=0;
        break;
    case 'F':
        s->cursor_row -= n < s->cursor_row ? n : s->cursor_row;
        s->cursor_column=0;
        break;
    case 'G': case '`':
        s->cursor_column=n<=s->columns?n-1:s->columns-1;
        break;
    case 'd':
        s->cursor_row=n<=s->rows?n-1:s->rows-1;
        break;
    case 'J':
        if (mode == 0) erase_range(s, cursor, total);
        if (mode == 1) erase_range(s, 0, cursor + 1);
        if (mode == 2) erase_range(s, 0, total);
        break;
    case 'K':
        if (mode == 0) erase_range(s, cursor, cursor - s->cursor_column + s->columns);
        if (mode == 1) erase_range(s, cursor - s->cursor_column, cursor + 1);
        if (mode == 2) erase_range(s, cursor - s->cursor_column, cursor - s->cursor_column + s->columns);
        /* A row erased whole is no longer the continuation of the row above
           (as insert-line already treats it): what is written there next is
           a new line, not more of the old one, when the width changes. */
        if ((mode == 2 || (mode == 0 && !s->cursor_column)) && s->cursor_row)
            s->cells[(size_t)s->cursor_row * s->columns - 1].wrap = 0;
        break;
    case 'm':
        for (i = 0; i < s->param_count; ++i) {
            n = s->params[i];
            if (n == 0) s->style = blank;
            else if (n == 1) s->style.bold = 1;
            else if (n == 4) s->style.colour_flags |= XCOLOUR_UNDERLINE;
            else if (n == 7) s->style.colour_flags |= XCOLOUR_INVERSE;
            else if (n == 27) s->style.colour_flags &= ~XCOLOUR_INVERSE;
            else if (n == 24) s->style.colour_flags &= ~XCOLOUR_UNDERLINE;
            else if (n == 22) s->style.bold = 0;
            else if (n >= 30 && n <= 37) {
                s->style.fg = (unsigned char)(n - 30);
                s->style.colour_flags &= ~(XCOLOUR_DEFAULT_FG | XCOLOUR_RGB_FG | XCOLOUR_PEN_FG);
                if (s->amiga_pens) s->style.colour_flags |= XCOLOUR_PEN_FG;
            } else if (n >= 40 && n <= 47) {
                s->style.bg = (unsigned char)(n - 40);
                s->style.colour_flags &= ~(XCOLOUR_DEFAULT_BG | XCOLOUR_RGB_BG | XCOLOUR_PEN_BG);
                if (s->amiga_pens) s->style.colour_flags |= XCOLOUR_PEN_BG;
            } else if (n >= 90 && n <= 97) {
                s->style.fg = (unsigned char)(n - 90 + 8);
                s->style.colour_flags &= ~(XCOLOUR_DEFAULT_FG | XCOLOUR_RGB_FG | XCOLOUR_PEN_FG);
            } else if (n >= 100 && n <= 107) {
                s->style.bg = (unsigned char)(n - 100 + 8);
                s->style.colour_flags &= ~(XCOLOUR_DEFAULT_BG | XCOLOUR_RGB_BG | XCOLOUR_PEN_BG);
            } else if (n == 39) {
                s->style.fg = 7;
                s->style.colour_flags = (s->style.colour_flags & ~(XCOLOUR_RGB_FG | XCOLOUR_PEN_FG)) |
                                        XCOLOUR_DEFAULT_FG;
            } else if (n == 49) {
                s->style.bg = 0;
                s->style.colour_flags = (s->style.colour_flags & ~(XCOLOUR_RGB_BG | XCOLOUR_PEN_BG)) |
                                        XCOLOUR_DEFAULT_BG;
            }
            else if (n == 38 || n == 48) {
                /* Stop on unsupported/incomplete payloads: a component such
                 * as 1 or 0 must never become bold or a style reset. */
                if (i + 1 >= s->param_count) break;
                if (s->params[i+1] == 5) {
                    if (i + 2 >= s->param_count || s->params[i+2] > 255) break;
                    if (n == 38) {
                        s->style.fg = (unsigned char)s->params[i+2];
                        s->style.colour_flags &= ~(XCOLOUR_DEFAULT_FG | XCOLOUR_RGB_FG | XCOLOUR_PEN_FG);
                    } else {
                        s->style.bg = (unsigned char)s->params[i+2];
                        s->style.colour_flags &= ~(XCOLOUR_DEFAULT_BG | XCOLOUR_RGB_BG | XCOLOUR_PEN_BG);
                    }
                    i += 2;
                } else if (s->params[i+1] == 2) {
                    uint32_t rgb;
                    if (i + 4 >= s->param_count || s->params[i+2] > 255 ||
                        s->params[i+3] > 255 || s->params[i+4] > 255) break;
                    rgb = (s->params[i+2] << 16) | (s->params[i+3] << 8) |
                          s->params[i+4];
                    if (n == 38) {
                        s->style.fg_rgb = rgb;
                        s->style.colour_flags = (s->style.colour_flags & ~(XCOLOUR_DEFAULT_FG | XCOLOUR_PEN_FG)) |
                                                XCOLOUR_RGB_FG;
                    } else {
                        s->style.bg_rgb = rgb;
                        s->style.colour_flags = (s->style.colour_flags & ~(XCOLOUR_DEFAULT_BG | XCOLOUR_PEN_BG)) |
                                                XCOLOUR_RGB_BG;
                    }
                    i += 4;
                } else break;
            }
        }
        return; /* SGR does not cancel a pending automatic wrap. */
    default: return;
    }
    s->wrap_pending = 0;
}

static void place_cluster(XTermScreen *s, XTermCell cell)
{
    unsigned width = cell.width;
    size_t cursor;
    if (s->wrap_pending) {
        s->cells[(size_t)(s->cursor_row + 1) * s->columns - 1].wrap = 1;
        s->cursor_column = 0; line_feed(s); s->wrap_pending = 0;
    }
    if (width > s->columns - s->cursor_column) {
        cursor = (size_t)s->cursor_row * s->columns + s->cursor_column;
        erase_range(s, cursor, cursor + 1);
        s->cells[(size_t)(s->cursor_row + 1) * s->columns - 1].wrap = 2;
        s->cursor_column = 0; line_feed(s);
    }
    cursor = (size_t)s->cursor_row * s->columns + s->cursor_column;
    erase_range(s, cursor, cursor + width);
    s->cells[cursor] = cell;
    s->cluster_cell = cursor;
    if (width == 2) {
        cell.ch = ' '; cell.width = 0; cell.length = 0;
        memset(cell.suffix, 0, sizeof cell.suffix);
        s->cells[cursor + 1] = cell;
    }
    if (s->cursor_column + width < s->columns) s->cursor_column += width;
    else { s->cursor_column = s->columns - 1; s->wrap_pending = 1; }
}

static void break_cluster(XTermScreen *s)
{
    s->cluster_active = 0;
    s->cluster_state = 0;
}

static void put_scalar(XTermScreen *s, uint32_t scalar)
{
    XTermCell cell;
    unsigned width = screen_scalar_width(scalar), overflow = 0;
    /* C1 controls are ignored, never interpreted as an 8-bit CSI. */
    if (scalar >= 0x80 && scalar < 0xa0) { break_cluster(s); return; }
    if (s->cluster_active && !utf8proc_grapheme_break_stateful(
            (int32_t)s->cluster_previous, (int32_t)scalar, &s->cluster_state)) {
        size_t index = s->cluster_cell;
        cell = s->cells[index];
        if (cell.length < 16) {
            unsigned old_width = cell.width;
            unsigned base_class = utf8proc_get_property((int32_t)cell.ch)->boundclass;
            unsigned i;
            cell.suffix[cell.length++ - 1] = scalar;
            /* Width is a terminal policy, separate from UAX #29 boundaries. */
            if ((scalar == 0xfe0f && (base_class == UTF8PROC_BOUNDCLASS_EXTENDED_PICTOGRAPHIC ||
                    cell.ch == '#' || cell.ch == '*' || (cell.ch >= '0' && cell.ch <= '9'))) ||
                (scalar == 0x20e3 && (cell.ch == '#' || cell.ch == '*' ||
                                    (cell.ch >= '0' && cell.ch <= '9')))) width = 2;
            if (utf8proc_get_property((int32_t)scalar)->boundclass ==
                    UTF8PROC_BOUNDCLASS_EXTENDED_PICTOGRAPHIC) {
                for (i = 0; i + 1 < cell.length; ++i)
                    if (cell.suffix[i] == 0x200d) width = 2;
            }
            if (width < old_width) width = old_width;
            cell.width = (unsigned char)width;
            if (width > s->columns) {
                cell.ch = 0xfffd; cell.length = 1; cell.width = 1;
                memset(cell.suffix, 0, sizeof cell.suffix);
                s->cells[index] = cell;
                break_cluster(s);
                return;
            }
            if (width != old_width) {
                /* Re-place the complete cluster from its lead. This also
                 * relocates a promoted last-column cell and scrolls safely. */
                erase_range(s, index, index + old_width);
                s->cursor_row = (unsigned)(index / s->columns);
                s->cursor_column = (unsigned)(index % s->columns);
                s->wrap_pending = 0;
                place_cluster(s, cell);
            } else s->cells[index] = cell;
            s->cluster_previous = scalar;
            return;
        }
        scalar = 0xfffd; width = 1; overflow = 1;
    }
    break_cluster(s);
    cell = s->style;
    cell.link_id = s->active_link;
    if (width > s->columns) { scalar = 0xfffd; width = 1; }
    cell.ch = scalar; cell.width = (unsigned char)width; cell.length = 1;
    memset(cell.suffix, 0, sizeof cell.suffix);
    place_cluster(s, cell);
    s->cluster_previous = scalar;
    s->cluster_active = !overflow;
}

/* Only the exact OSC 8 command is interpreted. Every other OSC is discarded
 * through its terminator, including malformed and overflowing payloads. */
static void osc_byte(XTermScreen *s,unsigned char ch)
{
    if(ch<32 || ch>=127) s->osc_invalid=1;
    if(s->osc_phase==0) s->osc_phase=ch=='8'?1:4;
    else if(s->osc_phase==1) s->osc_phase=ch==';'?2:4;
    else if(s->osc_phase==2) {
        if(ch==';') s->osc_phase=3;
        else if(s->osc_params>=XHYPERLINK_LIMIT-1) s->osc_invalid=1;
        else ++s->osc_params;
    } else if(s->osc_phase==3) {
        if(s->osc_length+1<XHYPERLINK_LIMIT)
            s->osc_target[s->osc_length++]=(char)ch;
        else s->osc_invalid=1;
    }
}

static void osc_end(XTermScreen *s)
{
    if(s->osc_phase==2 || s->osc_phase==3) {
        s->osc_target[s->osc_length]=0;
        if(s->osc_invalid || s->osc_phase!=3) s->active_link=XHYPERLINK_INVALID;
        else if(!s->osc_length) s->active_link=0;
        else s->active_link=hyperlink_register(s->links,s->osc_target);
    }
    s->parser=GROUND;
}

size_t screen_bulk_length(const XTermScreen *s, const unsigned char *bytes, size_t count)
{
    size_t n = 0;
    if (s->parser != APC) return 0;
    while (n < count && bytes[n] != 27 && bytes[n] != 24 && bytes[n] != 26) ++n;
    return n;
}

void screen_write(XTermScreen *s, const unsigned char *bytes, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        unsigned char ch = bytes[i];
        /* APC payload goes to the sink in runs: a Kitty frame is hundreds of
         * kilobytes and must not cost one call per byte. ESC, CAN and SUB end
         * a run and are handled below. */
        if (s->parser == APC && ch != 27 && ch != 24 && ch != 26) {
            size_t run = i;
            while (run < count && bytes[run] != 27 && bytes[run] != 24 && bytes[run] != 26) ++run;
            s->apc(s->apc_context, XSCREEN_APC_DATA, bytes + i, run - i);
            i = run - 1;
            continue;
        }
        if (s->utf8_remaining) {
            if (ch >= s->utf8_low && ch <= s->utf8_high) {
                s->utf8_value = (s->utf8_value << 6) | (ch & 0x3f);
                s->utf8_low = 0x80; s->utf8_high = 0xbf;
                ++s->visible_bytes;
                if (!--s->utf8_remaining) put_scalar(s, s->utf8_value);
                continue;
            }
            /* Replace the incomplete prefix, then process this byte again:
             * an ASCII control or new leading byte must not be swallowed. */
            s->utf8_remaining = 0;
            put_scalar(s, 0xfffd);
        }
        /* CAN/SUB abort any pending escape/control string without dispatching
         * its partial payload. Preserve already committed style/link state. */
        if(ch==24 || ch==26) {
            if(s->parser!=APC && s->parser!=APC_ESCAPE && s->parser!=STRING_DISCARD &&
               s->parser!=STRING_ESCAPE) ++s->visible_bytes;
            break_cluster(s);
            if((s->parser==APC || s->parser==APC_ESCAPE) && s->apc)
                s->apc(s->apc_context,XSCREEN_APC_ABORT,NULL,0);
            s->parser=GROUND;
            continue;
        }
        if(s->parser==APC) { s->parser=APC_ESCAPE; continue; } /* ch is ESC */
        if(s->parser==APC_ESCAPE) {
            /* Only ST ends the string. Any other escape inside it is malformed:
             * abort, and discard the rest so none of it becomes text. */
            if(ch=='\\') { s->apc(s->apc_context,XSCREEN_APC_END,NULL,0); s->parser=GROUND; }
            else {
                s->apc(s->apc_context,XSCREEN_APC_ABORT,NULL,0);
                s->parser=ch==27?STRING_ESCAPE:STRING_DISCARD;
            }
            continue;
        }
        /* Unsupported DCS/SOS/PM/APC: consume without interpreting or storing
         * payload. Only ST terminates; CAN/SUB abort even after a stray ESC.
         * Do not recognize raw C1 bytes in this UTF-8 terminal. */
        if(s->parser==STRING_DISCARD || s->parser==STRING_ESCAPE) {
            if(s->parser==STRING_ESCAPE && ch=='\\')
                s->parser=GROUND;
            else s->parser=ch==27?STRING_ESCAPE:STRING_DISCARD;
            continue;
        }
        /* From here a byte may change the display. An ESC counts with the
         * byte after it, so ESC _ (an APC string) counts nothing. */
        if (ch != 27) ++s->visible_bytes;
        if (s->parser == OSC || s->parser == OSC_ESCAPE) {
            if (ch == 7 || (s->parser == OSC_ESCAPE && ch == '\\')) {
                if(ch==7 && s->parser==OSC_ESCAPE) s->osc_invalid=1;
                osc_end(s);
            }
            else {
                if(s->parser==OSC_ESCAPE) s->osc_invalid=1;
                s->parser=ch==27?OSC_ESCAPE:OSC;
                if(ch!=27) osc_byte(s,ch);
            }
            continue;
        }
        /* NUL padding and DEL are ignored within ESC/CSI, not syntax errors.
         * Keep OSC validation above strict: controls inside URLs stay invalid. */
        if((ch==0 || ch==127) && (s->parser==ESCAPE || s->parser==CSI))
            continue;
        if (ch < 32 || ch == 127) break_cluster(s);
        if (ch == 27) {
            s->parser = ESCAPE; s->escape_reply_enabled=s->reply!=NULL; continue;
        }
        if (s->parser == ESCAPE) {
            if (ch == '[') {
                s->parser = CSI; s->param_count = 1; s->discard = 0;
                s->csi_private=s->param_started=s->csi_intermediate=0;
                s->csi_reply_enabled=s->escape_reply_enabled && s->reply!=NULL;
                memset(s->params, 0, sizeof s->params);
            } else if(ch=='_' && s->apc) {
                --s->visible_bytes;
                s->parser=APC;
                s->apc(s->apc_context,XSCREEN_APC_BEGIN,NULL,0);
            } else if(ch=='P' || ch=='X' || ch=='^' || ch=='_') {
                s->parser=STRING_DISCARD;
            } else if(ch=='M') {
                if(s->cursor_row==s->scroll_top) scroll_region_rows(s,1,1);
                else if(s->cursor_row) --s->cursor_row;
                s->wrap_pending=0;
                s->parser=GROUND;
            } else {
                s->parser = ch == ']' ? OSC : GROUND;
                s->osc_phase=s->osc_length=s->osc_invalid=s->osc_params=0;
            }
            continue;
        }
        if (s->parser == CSI) {
            if (ch >= '0' && ch <= '9') {
                if(s->csi_intermediate) s->discard=1;
                unsigned *p = &s->params[s->param_count - 1];
                s->param_started=1;
                /* Saturation bounds arithmetic even for hostile streams. */
                *p = *p < 100000 ? *p * 10 + ch - '0' : 1000000;
            } else if (ch == ';') {
                if(s->csi_intermediate) s->discard=1;
                s->param_started=1;
                if (s->param_count < 16) ++s->param_count;
                else s->discard = 1;
            } else if(ch=='?' && !s->param_started && !s->csi_private && !s->csi_intermediate) {
                s->csi_private=1;
            } else if(ch==' ' && !s->csi_intermediate) {
                s->csi_intermediate=ch;
            } else if (ch >= 0x40 && ch <= 0x7e) {
                execute_csi(s, ch); s->parser = GROUND;
            } else s->discard = 1;
            continue;
        }
        if (ch == '\r') { s->cursor_column = 0; s->wrap_pending = 0; }
        else if (ch == '\n') {
            s->cells[(size_t)(s->cursor_row + 1) * s->columns - 1].wrap = 0;
            s->wrap_pending = 0; line_feed(s);
        }
        else if (ch == '\b') {
            s->wrap_pending = 0;
            if (s->cursor_column) --s->cursor_column;
        } else if (ch == '\t') {
            unsigned next=(s->cursor_column/8+1)*8;
            s->cursor_column=next<s->columns?next:s->columns-1;
            s->wrap_pending=0;
        } else if (ch == '\f') {
            fill(s->cells, (size_t)s->rows * s->columns, s->style);
            s->cursor_row = s->cursor_column = s->wrap_pending = 0;
        } else if (ch >= 32 && ch <= 126) put_scalar(s, ch);
        else if (ch >= 0xc2 && ch <= 0xf4) {
            s->utf8_remaining = ch < 0xe0 ? 1 : ch < 0xf0 ? 2 : 3;
            s->utf8_value = ch & (ch < 0xe0 ? 0x1f : ch < 0xf0 ? 0x0f : 0x07);
            s->utf8_low = ch == 0xe0 ? 0xa0 : ch == 0xf0 ? 0x90 : 0x80;
            s->utf8_high = ch == 0xed ? 0x9f : ch == 0xf4 ? 0x8f : 0xbf;
        } else if (ch >= 0x80) put_scalar(s, 0xfffd);
    }
}

uint32_t screen_cell(const XTermScreen *s, unsigned row, unsigned column)
{
    if (!s || row >= s->rows || column >= s->columns) return ' ';
    return s->cells[(size_t)row * s->columns + column].ch;
}

void screen_scroll_view(XTermScreen *s, int delta)
{
    long offset = (long)s->view_offset + delta;
    if (offset < 0) offset = 0;
    if ((unsigned long)offset > s->history_count) offset = s->history_count;
    s->view_offset = (unsigned)offset;
}

XTermCell screen_view_cell(const XTermScreen *s, unsigned row, unsigned column)
{
    unsigned line;
    if (!s || row >= s->rows || column >= s->columns) return blank;
    if (row >= s->view_offset)
        return s->cells[(size_t)(row - s->view_offset) * s->columns + column];
    line = (s->history_head + s->history_lines - s->view_offset + row) % s->history_lines;
    return s->history[(size_t)line * s->columns + column];
}

XTermCell screen_line_cell(const XTermScreen *s, unsigned row, unsigned column)
{
    unsigned line;
    if (!s || column >= s->columns || row >= s->history_count + s->rows) return blank;
    if (row >= s->history_count)
        return s->cells[(size_t)(row - s->history_count) * s->columns + column];
    line = (s->history_head + s->history_lines - s->history_count + row) % s->history_lines;
    return s->history[(size_t)line * s->columns + column];
}
