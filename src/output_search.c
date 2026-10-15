#include "output_search.h"
#include "utf8proc.h"
#include <stdio.h>
#include <string.h>

/* Logical row r (0 = oldest kept) is absolute line base+r. */
static unsigned long base_line(const XTermScreen *s)
{ return s->history_pushed - s->history_count; }
static unsigned total_rows(const XTermScreen *s)
{ return s->history_count + s->rows; }

/* The last cell of a row ending in a soft wrap with padding is not text. */
static int padding(const XTermScreen *s, const XTermCell *row, unsigned c)
{ return c + 1 == s->columns && row[c].wrap == 2; }
static int wraps(const XTermScreen *s, const XTermCell *row)
{ return row[s->columns - 1].wrap != 0; }

static unsigned cell_scalars(const XTermCell *cell, uint32_t out[16])
{
    unsigned i, n;
    if (!cell->length) { out[0] = ' '; return 1; }
    n = cell->length > 16 ? 16 : cell->length;
    out[0] = cell->ch;
    for (i = 1; i < n; ++i) out[i] = cell->suffix[i - 1];
    return n;
}

static uint32_t fold(uint32_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* The next text cell after (r,c) on the same line: across a soft wrap,
 * never across a hard line end. 0 at the end of the line. */
static int next_cell(const XTermScreen *s, unsigned *r, unsigned *c)
{
    unsigned row = *r, col = *c;
    const XTermCell *cells = screen_line_row(s, row);
    for (;;) {
        ++col;
        if (col >= s->columns || padding(s, cells, col)) {
            if (!wraps(s, cells) || row + 1 >= total_rows(s)) return 0;
            ++row; col = 0; cells = screen_line_row(s, row);
            if (!cells) return 0;
        }
        if (cells[col].width) break;
    }
    *r = row; *c = col;
    return 1;
}

static int is_start(const XTermScreen *s, const XTermCell *row, unsigned c)
{ return row[c].width && !padding(s, row, c); }

/* Cells covered by a match starting at logical (r,c), or 0. */
static unsigned match_at(const XOutputSearch *q, const XTermScreen *s, unsigned r, unsigned c)
{
    unsigned used = 0, cells = 0;
    for (;;) {
        const XTermCell *row = screen_line_row(s, r);
        uint32_t text[16]; unsigned n, i;
        if (!row || !is_start(s, row, c)) return 0;
        n = cell_scalars(&row[c], text);
        if (n > q->scalar_count - used) return 0;
        for (i = 0; i < n; ++i)
            if (fold(text[i]) != fold(q->scalars[used + i])) return 0;
        used += n; ++cells;
        if (used == q->scalar_count) return cells;
        if (!next_cell(s, &r, &c)) return 0;
    }
}

/* Every match, oldest first. visit returns nonzero to stop. */
typedef int (*Visit)(void *, unsigned long line, unsigned column, unsigned cells);
static void each_match(const XOutputSearch *q, const XTermScreen *s,
                       unsigned first_row, unsigned end_row, Visit visit, void *context)
{
    unsigned r, c, n, rows = total_rows(s);
    unsigned long base = base_line(s);
    if (!q->scalar_count || s->alternate_active) return;
    if (end_row > rows) end_row = rows;
    for (r = first_row; r < end_row; ++r) {
        const XTermCell *row = screen_line_row(s, r);
        if (!row) return;
        for (c = 0; c < s->columns; ++c)
            if ((n = match_at(q, s, r, c)) && visit(context, base + r, c, n)) return;
    }
}

static int before(unsigned long l1, unsigned c1, unsigned long l2, unsigned c2)
{ return l1 < l2 || (l1 == l2 && c1 < c2); }

typedef struct {
    unsigned long line; unsigned column;   /* the reference point */
    int strict;                            /* < instead of <= */
    unsigned long count, at_or_before;     /* matches; matches up to the point */
    int have_last, have_next;
    unsigned long last_line, next_line; unsigned last_column, next_column, last_cells, next_cells;
    unsigned long wanted;                  /* stop at the wanted-th, 1-based; 0 none */
    int have_wanted; unsigned long wanted_line; unsigned wanted_column, wanted_cells;
} Survey;

static int survey_visit(void *context, unsigned long line, unsigned column, unsigned cells)
{
    Survey *v = context;
    ++v->count;
    if (v->strict ? before(line, column, v->line, v->column)
                  : !before(v->line, v->column, line, column)) {
        ++v->at_or_before; v->have_last = 1;
        v->last_line = line; v->last_column = column; v->last_cells = cells;
    } else if (!v->have_next) {
        v->have_next = 1; v->next_line = line; v->next_column = column; v->next_cells = cells;
    }
    if (v->wanted && v->count == v->wanted) {
        v->have_wanted = 1; v->wanted_line = line; v->wanted_column = column; v->wanted_cells = cells;
    }
    return 0;
}

static Survey survey(const XOutputSearch *q, const XTermScreen *s,
                     unsigned long line, unsigned column, int strict, unsigned long wanted)
{
    Survey v;
    memset(&v, 0, sizeof v);
    v.line = line; v.column = column; v.strict = strict; v.wanted = wanted;
    each_match(q, s, 0, total_rows(s), survey_visit, &v);
    return v;
}

static void set_current(XOutputSearch *q, unsigned long line, unsigned column,
                        unsigned cells, unsigned long index, unsigned long total)
{
    q->found = 1; q->line = line; q->column = column; q->cells = cells;
    q->index = index; q->total = total;
}

/* The newest match at or before (line,column); else the oldest after it. */
static void locate(XOutputSearch *q, const XTermScreen *s, unsigned long line, unsigned column)
{
    Survey v = survey(q, s, line, column, 0, 0);
    q->total = v.count;
    if (v.have_last) set_current(q, v.last_line, v.last_column, v.last_cells, v.at_or_before, v.count);
    else if (v.have_next) set_current(q, v.next_line, v.next_column, v.next_cells, 1, v.count);
    else { q->found = 0; q->index = 0; }
}

/* Put the current match in view if it is not, a third from the top. */
static void reveal(XOutputSearch *q, XTermScreen *s)
{
    unsigned long base = base_line(s), top, row;
    unsigned want;
    if (!q->found || q->line < base) return;
    row = q->line - base;
    top = s->history_count - s->view_offset;
    if (row >= top && row < top + s->rows) return;
    top = row > s->rows / 3 ? row - s->rows / 3 : 0;
    if (top > s->history_count) top = s->history_count;
    want = (unsigned)(s->history_count - top);
    screen_scroll_view(s, (int)want - (int)s->view_offset);
}

static void seen(XOutputSearch *q, const XTermScreen *s)
{
    q->pushed_seen = s->history_pushed; q->layout_seen = s->layout_serial;
    q->visible_seen = s->visible_bytes; q->offset_seen = s->view_offset;
}

/* The bottom of the view when the search opened: where a new query starts. */
static unsigned long origin_line(const XOutputSearch *q, const XTermScreen *s)
{
    unsigned long bottom = base_line(s) + s->history_count + s->rows - 1;
    unsigned long offset = q->restore_offset;
    if (q->layout_seen != s->layout_serial) return bottom;
    if (offset) offset += s->history_pushed - q->restore_pushed;
    return offset > bottom ? 0 : bottom - offset;
}

XSearchOpen output_search_open(XOutputSearch *q, XTermScreen *s)
{
    memset(q, 0, sizeof *q);
    if (s->alternate_active) return XSEARCH_ALTERNATE;
    if (!s->history || !s->history_lines) return XSEARCH_NO_HISTORY;
    q->open = 1;
    q->restore_offset = s->view_offset;
    q->restore_pushed = s->history_pushed;
    seen(q, s);
    return XSEARCH_OPENED;
}

static int decode(const char *text, size_t n, uint32_t *out, unsigned room, unsigned *count)
{
    size_t pos = 0; unsigned k = 0;
    while (pos < n) {
        utf8proc_int32_t scalar;
        utf8proc_ssize_t used = utf8proc_iterate((const utf8proc_uint8_t *)text + pos,
                                                 (utf8proc_ssize_t)(n - pos), &scalar);
        if (used <= 0 || scalar < 32 || (scalar >= 127 && scalar <= 159) || k >= room) return 0;
        out[k++] = (uint32_t)scalar; pos += (size_t)used;
    }
    *count = k;
    return 1;
}

static void search_again(XOutputSearch *q, XTermScreen *s, int from_current)
{
    unsigned long line = from_current && q->found ? q->line : origin_line(q, s);
    unsigned column = from_current && q->found ? q->column : s->columns;
    locate(q, s, line, column);
    reveal(q, s);
    seen(q, s);
}

int output_search_insert(XOutputSearch *q, XTermScreen *s, const char *text, size_t n)
{
    unsigned added;
    if (!q->open || !n || !text || n > XSEARCH_QUERY_MAX - 1 - q->query_length) return 0;
    if (!decode(text, n, q->scalars + q->scalar_count, XSEARCH_SCALARS - q->scalar_count, &added))
        return 0;
    memcpy(q->query + q->query_length, text, n);
    q->query_length += n; q->query[q->query_length] = 0;
    q->scalar_count += added;
    /* A longer query still matches the result shown, if anything does. */
    search_again(q, s, 1);
    return 1;
}

int output_search_backspace(XOutputSearch *q, XTermScreen *s)
{
    if (!q->open || !q->query_length) return 0;
    do --q->query_length;
    while (q->query_length && ((unsigned char)q->query[q->query_length] & 0xc0) == 0x80);
    q->query[q->query_length] = 0;
    --q->scalar_count;
    if (!q->scalar_count) { q->found = 0; q->index = q->total = 0; seen(q, s); return 1; }
    search_again(q, s, 0);
    return 1;
}

int output_search_step(XOutputSearch *q, XTermScreen *s, int older)
{
    Survey v;
    if (!q->open || !q->found) return 0;
    v = survey(q, s, q->line, q->column, 1, 0);
    q->total = v.count;
    if (older) {
        if (!v.have_last) return 0;
        set_current(q, v.last_line, v.last_column, v.last_cells, v.at_or_before, v.count);
    } else {
        /* survey's "next" is the first match not strictly before: the
           current one itself. The one after it is the next newer. */
        Survey w = survey(q, s, q->line, q->column, 0, 0);
        if (!w.have_next) return 0;
        set_current(q, w.next_line, w.next_column, w.next_cells, w.at_or_before + 1, w.count);
    }
    reveal(q, s);
    seen(q, s);
    return 1;
}

void output_search_close(XOutputSearch *q, XTermScreen *s, int restore)
{
    if (!q->open) return;
    if (restore && !s->alternate_active) {
        unsigned long offset = q->restore_offset;
        if (offset && q->layout_seen == s->layout_serial)
            offset += s->history_pushed - q->restore_pushed;
        if (offset > s->history_count) offset = s->history_count;
        screen_scroll_view(s, (int)offset - (int)s->view_offset);
    }
    q->open = 0;
}

int output_search_follow(XOutputSearch *q, XTermScreen *s)
{
    if (!q->open) return 0;
    if (s->alternate_active) { q->open = 0; return 1; }
    if (q->layout_seen != s->layout_serial) {
        /* A reflow keeps the text, and so the order of the matches; the
           current one is the same place counted from the newest. */
        unsigned long from_end = q->found ? q->total - q->index : 0;
        int had = q->found;
        Survey v = survey(q, s, 0, 0, 1, 0);
        q->total = v.count;
        if (had && v.count) {
            unsigned long k = v.count > from_end ? v.count - from_end : 1;
            Survey w = survey(q, s, 0, 0, 1, k);
            if (w.have_wanted) set_current(q, w.wanted_line, w.wanted_column, w.wanted_cells, k, v.count);
        } else q->found = 0;
        if (q->restore_offset > s->history_count) q->restore_offset = s->history_count;
        q->restore_pushed = s->history_pushed;
        reveal(q, s);
        seen(q, s);
        return 0;
    }
    /* The model keeps a scrolled-back view on its text; the live view
       (offset 0) would follow the output, so hold it here. */
    if (s->history_pushed != q->pushed_seen && !q->offset_seen && !s->view_offset)
        screen_scroll_view(s, (int)(s->history_pushed - q->pushed_seen));
    if (s->history_pushed != q->pushed_seen || s->visible_bytes != q->visible_seen) {
        unsigned long base = base_line(s);
        if (q->found) {
            unsigned n = q->line >= base && q->line - base < total_rows(s) ?
                match_at(q, s, (unsigned)(q->line - base), q->column) : 0;
            if (n == q->cells) {
                Survey v = survey(q, s, q->line, q->column, 0, 0);
                q->index = v.at_or_before; q->total = v.count;
            } else locate(q, s, q->line < base ? base : q->line, q->line < base ? 0 : q->column);
        } else if (q->scalar_count)
            /* New text matched: offer it, without moving the view. */
            locate(q, s, origin_line(q, s), s->columns);
    }
    seen(q, s);
    return 0;
}

typedef struct {
    const XOutputSearch *q; const XTermScreen *s;
    unsigned char *marks; unsigned long top; /* absolute line of view row 0 */
} Marking;

static int mark_visit(void *context, unsigned long line, unsigned column, unsigned cells)
{
    Marking *m = context;
    const XTermScreen *s = m->s;
    unsigned long base = base_line(s);
    unsigned r = (unsigned)(line - base), c = column, i;
    unsigned char value = m->q->found && line == m->q->line && column == m->q->column ? 2 : 1;
    for (i = 0; i < cells; ++i) {
        unsigned long at = base + r;
        if (at >= m->top && at < m->top + s->rows) {
            unsigned char *cell = m->marks + (size_t)(at - m->top) * s->columns + c;
            if (*cell < value) *cell = value;
        }
        if (i + 1 < cells && !next_cell(s, &r, &c)) break;
    }
    return 0;
}

void output_search_marks(const XOutputSearch *q, const XTermScreen *s,
                         unsigned char *marks, size_t capacity)
{
    Marking m;
    unsigned top_row, first, guard;
    size_t need = (size_t)s->rows * s->columns;
    if (capacity < need) return;
    memset(marks, 0, need);
    if (!q->open || !q->scalar_count || s->alternate_active) return;
    top_row = s->history_count - s->view_offset;
    /* A match can start above the view on a wrapped line that runs into it. */
    first = top_row;
    for (guard = 0; first && guard < XSEARCH_SCALARS; ++guard) {
        const XTermCell *previous = screen_line_row(s, first - 1);
        if (!previous || !wraps(s, previous)) break;
        --first;
    }
    m.q = q; m.s = s; m.marks = marks; m.top = base_line(s) + top_row;
    each_match(q, s, first, top_row + s->rows, mark_visit, &m);
}

size_t output_search_label(const XOutputSearch *q, char *out, size_t capacity)
{
    char tail[48]; size_t keep, room;
    int n;
    if (!capacity) return 0;
    if (!q->scalar_count) n = snprintf(tail, sizeof tail, "type to search, Esc closes");
    else if (!q->found) n = snprintf(tail, sizeof tail, " - not found");
    else n = snprintf(tail, sizeof tail, " - %lu of %lu", q->index, q->total);
    if (n < 0) n = 0;
    room = capacity - 1;
    if (room < 6 + (size_t)n) { out[0] = 0; return 0; }
    keep = q->query_length;
    if (keep > room - 6 - (size_t)n) {
        keep = room - 6 - (size_t)n;
        while (keep && ((unsigned char)q->query[keep] & 0xc0) == 0x80) --keep;
    }
    memcpy(out, "Find: ", 6);
    memcpy(out + 6, q->query, keep);
    memcpy(out + 6 + keep, tail, (size_t)n);
    out[6 + keep + n] = 0;
    return 6 + keep + (size_t)n;
}
