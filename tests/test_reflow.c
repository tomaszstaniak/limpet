/* Width changes keep text and scrollback: soft-wrapped lines are rejoined
 * and wrapped again at the new width (screen_attach_reflow). */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CAP (40 * 40)
#define HIST (64 * 40)
static XTermScreen s;
static XTermCell cells[CAP], alt[CAP], history[HIST], spare[HIST];

static void text(const char *p) { screen_write(&s, (const unsigned char *)p, strlen(p)); }

static void setup(unsigned rows, unsigned columns)
{
    assert(!screen_init(&s, cells, CAP, rows, columns));
    assert(!screen_attach_history(&s, history, HIST));
    assert(!screen_attach_alternate(&s, alt, CAP));
    assert(!screen_attach_reflow(&s, spare, HIST));
}

/* Every logical line, oldest first, joined across soft wraps, trailing
 * blanks dropped; lines separated by '|'. ASCII and '#' for other scalars. */
static const char *logical(void)
{
    static char out[8192];
    size_t n = 0, line_start = 0;
    unsigned total = s.history_count + s.rows, r, c;
    for (r = 0; r < total; ++r) {
        XTermCell tail = screen_line_cell(&s, r, s.columns - 1);
        unsigned width = tail.wrap == 2 ? s.columns - 1 : s.columns;
        for (c = 0; c < width; ++c) {
            XTermCell cell = screen_line_cell(&s, r, c);
            if (cell.width == 0) continue;
            out[n++] = cell.ch < 128 ? (char)cell.ch : '#';
        }
        if (!tail.wrap) {
            while (n > line_start && out[n - 1] == ' ') --n;
            out[n++] = '|'; line_start = n;
        }
    }
    while (n && (out[n - 1] == '|')) --n;
    out[n] = 0;
    return out;
}

static void narrow_and_back(void)
{
    setup(4, 10);
    text("abcdefghij");
    text("\r\nab\r\ncd");
    assert(!strcmp(logical(), "abcdefghij|ab|cd"));
    assert(!screen_resize(&s, 4, 4));
    /* abcd efgh ij ab cd: two rows went to history */
    assert(!strcmp(logical(), "abcdefghij|ab|cd"));
    assert(screen_cell(&s, s.rows - 1, 0) == 'c');
    assert(!screen_resize(&s, 4, 10));
    assert(!strcmp(logical(), "abcdefghij|ab|cd"));
    assert(screen_cell(&s, 0, 9) == 'j' && s.history_count == 0);
}

static void scrollback_kept(void)
{
    char want[2048] = "", line[32];
    unsigned i;
    setup(5, 10);
    for (i = 0; i < 30; ++i) {
        snprintf(line, sizeof line, "%sline%02u", i ? "\r\n" : "", i);
        text(line);
        snprintf(line, sizeof line, "%sline%02u", i ? "|" : "", i);
        strcat(want, line);
    }
    assert(!strcmp(logical(), want));
    assert(!screen_resize(&s, 5, 4));
    assert(!strcmp(logical(), want));
    assert(!screen_resize(&s, 5, 20));
    assert(!strcmp(logical(), want));
    assert(!screen_resize(&s, 3, 7));
    assert(!strcmp(logical(), want));
}

static void cursor_after_prompt(void)
{
    setup(4, 10);
    text("first\r\nPROMPT> ");
    assert(!screen_resize(&s, 4, 5));
    text("x");
    assert(!strcmp(logical(), "first|PROMPT> x"));
    assert(!screen_resize(&s, 4, 12));
    text("y");
    assert(!strcmp(logical(), "first|PROMPT> xy"));
    assert(s.cursor_row == 1 && s.cursor_column == 10);
}

static void pending_wrap(void)
{
    /* the cursor waits after the last cell of a full row */
    setup(4, 10);
    text("0123456789");
    assert(s.wrap_pending);
    assert(!screen_resize(&s, 4, 7));
    text("X");
    assert(!strcmp(logical(), "0123456789X"));
    assert(!screen_resize(&s, 4, 20));
    text("Y");
    assert(!strcmp(logical(), "0123456789XY"));
}

static void erased_tail(void)
{
    /* AROS One's prompt leaves pen 0 as the background; an erase to the end
     * of the line then fills the row with "blank" cells in that colour.
     * 16 characters at 8 columns are two rows; the 20 erased cells would
     * make a third, empty one before the prompt. */
    setup(6, 20);
    s.amiga_pens = 1;
    text("0123456789012345\033[40m\033[K\r\n\033[0mp> ");
    assert(!screen_resize(&s, 6, 8));
    assert(!strcmp(logical(), "0123456789012345|p>"));
    assert(screen_cell(&s, s.cursor_row, 0) == 'p' &&
           screen_cell(&s, s.cursor_row - 1, 0) == '8');
}

static void wide_and_clusters(void)
{
    XTermCell c;
    unsigned r, found = 0;
    setup(4, 6);
    text("a\xe6\xbc\xa2" "b e\xcc\x81");   /* a, U+6F22 (wide), b, space, e+U+0301 */
    assert(!screen_resize(&s, 4, 2));
    /* "a" then the wide glyph cannot share a 2-cell row with it: padded */
    assert(!strcmp(logical(), "a#b e"));
    for (r = 0; r < s.history_count + s.rows; ++r) {
        c = screen_line_cell(&s, r, 0);
        if (c.ch == 'e') { assert(c.length == 2 && c.suffix[0] == 0x301); found = 1; }
    }
    assert(found);
    assert(!screen_resize(&s, 4, 6));
    assert(!strcmp(logical(), "a#b e"));
    assert(screen_cell(&s, 0, 1) == 0x6F22 && screen_line_cell(&s, 0, 2).width == 0);
}

static void attributes_kept(void)
{
    setup(3, 8);
    text("\033[31mred\033[0m \033]8;;https://x.example\033\\link\033]8;;\033\\");
    assert(!screen_resize(&s, 3, 3));
    assert(!screen_resize(&s, 3, 8));
    assert(screen_cell(&s, 0, 0) == 'r' && cells[0].fg == 1 &&
           !(cells[0].colour_flags & XCOLOUR_DEFAULT_FG));
    assert(screen_cell(&s, 0, 4) == 'l' && cells[4].link_id != 0);
}

static void alternate_then_back(void)
{
    setup(4, 10);
    text("abcdefghij\r\nshell> ");
    text("\033[?1049hFULLSCREEN");
    assert(!screen_resize(&s, 4, 4));
    text("\033[?1049l");
    assert(!strcmp(logical(), "abcdefghij|shell>"));
    text("z");
    assert(!strcmp(logical(), "abcdefghij|shell> z"));
}

/* Property: any sequence of widths keeps the logical text. */
static void random_widths(void)
{
    static const char *pieces[] = { "a", "bc", "def", " ", "\xe6\xbc\xa2", "xyz12", "\r\n" };
    unsigned seed = 12345, round, i;
    for (round = 0; round < 300; ++round) {
        char before[8192];
        unsigned rows = 3 + seed % 5, columns = 3 + (seed >> 3) % 12;
        setup(rows, columns);
        for (i = 0; i < 40; ++i) {
            seed = seed * 1103515245u + 12345u;
            text(pieces[(seed >> 16) % 7]);
        }
        strcpy(before, logical());
        for (i = 0; i < 6; ++i) {
            seed = seed * 1103515245u + 12345u;
            columns = 2 + (seed >> 16) % 30;
            rows = 2 + (seed >> 8) % 6;
            assert(!screen_resize(&s, rows, columns));
            if (strcmp(logical(), before)) {
                printf("round %u resize %u: %ux%u\n  before %s\n  after  %s\n",
                       round, i, rows, columns, before, logical());
                assert(0);
            }
        }
    }
}

static void without_spare_unchanged(void)
{
    /* No reflow storage: the old crop behaviour, history cleared. */
    assert(!screen_init(&s, cells, CAP, 4, 10));
    assert(!screen_attach_history(&s, history, HIST));
    text("abcdefghij");
    assert(!screen_resize(&s, 4, 4));
    assert(screen_cell(&s, 0, 3) == 'd' && screen_cell(&s, 1, 0) == ' ');
}

int main(void)
{
    narrow_and_back();
    scrollback_kept();
    cursor_after_prompt();
    pending_wrap();
    erased_tail();
    wide_and_clusters();
    attributes_kept();
    alternate_then_back();
    random_widths();
    without_spare_unchanged();
    puts("REFLOW PASS: rejoin/rewrap, scrollback, cursor, pending wrap, erased tail, wide/clusters, attributes, alternate, 300 random width sequences, no spare");
    return 0;
}
