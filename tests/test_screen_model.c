#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermCell cells[128], history[32], alternate[128];
static XTermScreen s;
static void init(unsigned r, unsigned c)
{
    assert(screen_init(&s, cells, 128, r, c) == 0);
}
static void write_text(const char *text)
{
    screen_write(&s, (const unsigned char *)text, strlen(text));
}
static void row(unsigned r, const char *text)
{
    unsigned c;
    assert(strlen(text) == s.columns);
    for (c = 0; c < s.columns; ++c)
        assert(screen_cell(&s, r, c) == (unsigned char)text[c]);
}
static void valid_pairs(void)
{
    unsigned r, c;
    for (r = 0; r < s.rows; ++r) {
        for (c = 0; c < s.columns; ++c) {
            XTermCell cell = screen_view_cell(&s, r, c);
            assert(cell.width <= 2);
            assert(cell.length <= 16);
            if (!cell.length) assert(cell.ch == ' ');
            {
                unsigned suffix;
                for (suffix = cell.length ? cell.length - 1 : 0; suffix < 15; ++suffix)
                    assert(cell.suffix[suffix] == 0);
            }
            if (cell.width == 0) {
                XTermCell lead;
                assert(c > 0 && cell.ch == ' ');
                assert(cell.length == 0);
                lead = screen_view_cell(&s, r, c - 1);
                assert(lead.width == 2);
                assert(lead.fg == cell.fg && lead.bg == cell.bg && lead.bold == cell.bold);
            } else if (cell.width == 2) {
                assert(c + 1 < s.columns);
                assert(screen_view_cell(&s, r, c + 1).width == 0);
            }
        }
    }
}
/* SGR 4 must persist on cells independently of colour changes and be cleared
 * only by SGR 24 or a complete reset. The renderer consumes this cell state. */
static void underline_style(void)
{
    unsigned char plain, rgb_plain, marked;
    size_t split;
    const char *seq="\033[4;38;2;17;34;51mA";
    init(2,8);
    plain=s.style.colour_flags;
    write_text("\033[38;2;17;34;51mA");
    rgb_plain=cells[0].colour_flags;
    for(split=0;split<=strlen(seq);++split) {
        init(2,8);
        screen_write(&s,(const unsigned char *)seq,split);
        screen_write(&s,(const unsigned char *)seq+split,strlen(seq)-split);
        marked=cells[0].colour_flags;
        assert(marked!=rgb_plain);
        assert(s.style.colour_flags==marked);
    }
    write_text("\033[24mB\033[4;39;49mC\033[0mD");
    assert(cells[1].colour_flags==rgb_plain);
    assert(cells[2].colour_flags!=plain);
    assert(cells[3].colour_flags==plain);
    init(2,8);
    write_text("\033[4m中");
    assert(cells[0].colour_flags==cells[1].colour_flags);
    assert(cells[0].colour_flags!=s.cells[2].colour_flags);
}
/* DECSTBM must let an application stream logs without moving its footer. */
static void scrolling_region(void)
{
    const char *set = "\033[2;4r";
    size_t split;
    for (split = 0; split <= strlen(set); ++split) {
        init(5, 4);
        assert(screen_attach_history(&s, history, 8) == 0);
        write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
        screen_write(&s, (const unsigned char *)set, split);
        screen_write(&s, (const unsigned char *)set + split, strlen(set) - split);
        write_text("\033[4;1H\n");
        row(0, "AAAA"); row(1, "CCCC"); row(2, "DDDD");
        row(3, "    "); row(4, "EEEE");
        assert(s.history_count == 0);
        assert(s.cursor_row == 3 && s.cursor_column == 0);
    }
    /* A reversed or out-of-range region must not replace the active one. */
    write_text("\033[4;2r\033[2;6r\033[4;1H\n");
    row(0, "AAAA"); row(1, "DDDD"); row(2, "    ");
    row(3, "    "); row(4, "EEEE");
    assert(s.history_count == 0);

    /* Automatic wrap uses the same region as an explicit line feed. */
    init(5, 4);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[4;4HZx");
    row(0, "AAAA"); row(1, "CCCC"); row(2, "DDDZ");
    row(3, "x   "); row(4, "EEEE");

    /* Resetting the margins restores the ordinary full-screen scrollback. */
    init(5, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[4;1H\n\033[4;1H\n");
    write_text("\033[r\033[5;1H\n");
    row(0, "DDDD"); row(3, "EEEE"); row(4, "    ");
    assert(s.history_count == 1);
    assert(screen_line_cell(&s, 0, 0).ch == 'A');

    /* Alternate-screen margins do not leak back into the primary screen. */
    init(5, 4);
    assert(screen_attach_alternate(&s, alternate, 128) == 0);
    write_text("\033[2;4r\033[?1049h\033[2;3r\033[?1049l");
    assert(s.scroll_top == 1 && s.scroll_bottom == 3);
    write_text("\033[?1049h");
    assert(s.scroll_top == 0 && s.scroll_bottom == 4);
    write_text("\033[?1049l");

    /* A resize invalidates both screens' prior margins. */
    write_text("\033[2;4r");
    assert(screen_resize(&s, 4, 4) == 0);
    assert(s.scroll_top == 0 && s.scroll_bottom == 3);
    write_text("\033[2;3r\033[?1049h");
    assert(screen_resize(&s, 3, 4) == 0);
    write_text("\033[?1049l");
    assert(s.scroll_top == 0 && s.scroll_bottom == 2);
}
/* IL must shift only rows from the cursor to the bottom margin, not the
 * application header/footer or the primary scrollback ring. */
static void insert_lines(void)
{
    const char *two = "\033[2L";
    size_t split;
    init(5, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[3;2H\033[L");
    row(0, "AAAA"); row(1, "BBBB"); row(2, "    ");
    row(3, "CCCC"); row(4, "EEEE");
    assert(s.cursor_row == 2 && s.cursor_column == 0);
    assert(s.history_count == 0);
    for (split = 0; split <= strlen(two); ++split) {
        init(5, 4);
        write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
        write_text("\033[2;4r\033[3;2H");
        screen_write(&s, (const unsigned char *)two, split);
        screen_write(&s, (const unsigned char *)two + split, strlen(two) - split);
        row(0, "AAAA"); row(1, "BBBB"); row(2, "    ");
        row(3, "    "); row(4, "EEEE");
    }
    /* A cursor outside the margins cannot modify the header. */
    write_text("\033[1;2H\033[999L");
    row(0, "AAAA"); row(4, "EEEE");
    assert(s.cursor_row == 0 && s.cursor_column == 1);

    /* Inserting a blank row breaks a soft-wrap link to the former neighbor. */
    init(5, 4);
    write_text("\033[2;4r\033[2;1HABCDX");
    assert(s.cells[2 * s.columns - 1].wrap == 1);
    write_text("\033[3;1H\033[L");
    assert(s.cells[2 * s.columns - 1].wrap == 0);
}
/* DL must pull later rows upward and blank the bottom of the region. */
static void delete_lines(void)
{
    init(5, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[2;2H\033[M");
    row(0, "AAAA"); row(1, "CCCC"); row(2, "DDDD");
    row(3, "    "); row(4, "EEEE");
    assert(s.cursor_row == 1 && s.cursor_column == 0);
    assert(s.history_count == 0);
    /* Zero means one; oversized counts blank only the remaining region. */
    init(5, 4);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[2;1H\033[0M");
    row(1, "CCCC"); row(2, "DDDD"); row(3, "    ");
    write_text("\033[2;1H\033[999M");
    row(0, "AAAA"); row(1, "    "); row(2, "    ");
    row(3, "    "); row(4, "EEEE");

    /* Whole rows carry both halves of a wide glyph without breaking pairs. */
    init(5, 4);
    write_text("\033[3;1H中x\033[4;1Htest\033[5;1HEEEE");
    write_text("\033[2;4r\033[2;1H\033[M");
    assert(screen_cell(&s, 1, 0) == 0x4e2d);
    row(2, "test"); row(4, "EEEE");
    valid_pairs();
}
/* CSI S/T scroll the selected region without moving its pinned neighbors. */
static void explicit_region_scroll(void)
{
    init(5, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[1;2H\033[S");
    row(0, "AAAA"); row(1, "CCCC"); row(2, "DDDD");
    row(3, "    "); row(4, "EEEE");
    assert(s.cursor_row == 0 && s.cursor_column == 1);
    assert(s.history_count == 0);
    write_text("\033[T");
    row(0, "AAAA"); row(1, "    "); row(2, "CCCC");
    row(3, "DDDD"); row(4, "EEEE");
    assert(s.history_count == 0);
}
/* Reverse index inserts only at the top margin; elsewhere it moves the cursor. */
static void reverse_index(void)
{
    init(5, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("AAAA\033[2;1HBBBB\033[3;1HCCCC\033[4;1HDDDD\033[5;1HEEEE");
    write_text("\033[2;4r\033[2;2H\033M");
    row(0, "AAAA"); row(1, "    "); row(2, "BBBB");
    row(3, "CCCC"); row(4, "EEEE");
    assert(s.cursor_row == 1 && s.cursor_column == 1);
    assert(s.history_count == 0);
    write_text("\033[4;2H\033M");
    assert(s.cursor_row == 2 && s.cursor_column == 1);
    row(1, "    "); row(2, "BBBB"); row(3, "CCCC");
}
/* Row r of history + screen, as screen_line_cell sees it. */
static void line(unsigned r, const char *text)
{
    unsigned c;
    assert(strlen(text) == s.columns);
    for (c = 0; c < s.columns; ++c)
        assert(screen_line_cell(&s, r, c).ch == (unsigned char)text[c]);
}
/* Shrinking the height keeps the cursor row (the prompt) visible. Rows taken
 * off the top go to scrollback on the primary screen when the width is
 * unchanged. A width change still discards history (no reflow), and alternate
 * screen rows never enter scrollback. */
static void resize_keeps_cursor_row(void)
{
    /* Height only, cursor on the last row: top rows move into history. */
    init(4, 4);
    assert(screen_attach_history(&s, history, 32) == 0);
    write_text("aaaa\r\nbbbb\r\ncccc\r\ndd");
    assert(s.cursor_row == 3 && s.cursor_column == 2 && s.history_count == 0);
    assert(screen_resize(&s, 2, 4) == 0);
    row(0, "cccc"); row(1, "dd  ");
    assert(s.cursor_row == 1 && s.cursor_column == 2);
    assert(s.history_count == 2);
    line(0, "aaaa"); line(1, "bbbb"); line(2, "cccc"); line(3, "dd  ");
    write_text("X");
    row(1, "ddX ");

    /* Growing adds blank rows below and leaves history alone; shrinking
     * while the cursor still fits moves nothing. */
    assert(screen_resize(&s, 4, 4) == 0);
    row(0, "cccc"); row(1, "ddX "); row(2, "    "); row(3, "    ");
    assert(s.history_count == 2 && s.cursor_row == 1);
    assert(screen_resize(&s, 3, 4) == 0);
    row(0, "cccc"); row(1, "ddX ");
    assert(s.history_count == 2 && s.cursor_row == 1);
    /* A second shrink past the cursor pushes one more row. */
    assert(screen_resize(&s, 1, 4) == 0);
    row(0, "ddX ");
    assert(s.cursor_row == 0 && s.history_count == 3);
    line(2, "cccc"); line(3, "ddX ");

    /* A cursor that already fits: plain crop, no history. */
    init(4, 4);
    assert(screen_attach_history(&s, history, 32) == 0);
    write_text("aaaa\r\nbb");
    assert(screen_resize(&s, 2, 4) == 0);
    row(0, "aaaa"); row(1, "bb  ");
    assert(s.cursor_row == 1 && s.history_count == 0);

    /* A scrolled-back view stays on the same content. */
    init(3, 4);
    assert(screen_attach_history(&s, history, 32) == 0);
    write_text("aaaa\r\nbbbb\r\ncccc\r\ndddd\r\ne");
    assert(s.history_count == 2);
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 0).ch == 'b');
    assert(screen_resize(&s, 2, 4) == 0);
    assert(s.history_count == 3 && s.view_offset == 2);
    assert(screen_view_cell(&s, 0, 0).ch == 'b');

    /* Height and width together: the cursor row stays, history is dropped. */
    init(4, 4);
    assert(screen_attach_history(&s, history, 32) == 0);
    write_text("aaaa\r\nbbbb\r\ncccc\r\ndd");
    assert(screen_resize(&s, 2, 6) == 0);
    row(0, "cccc  "); row(1, "dd    ");
    assert(s.cursor_row == 1 && s.cursor_column == 2 && s.history_count == 0);

    /* Alternate screen: its rows shift but never reach history; the saved
     * primary screen follows the same rule and its rows do. */
    init(4, 4);
    assert(screen_attach_history(&s, history, 32) == 0);
    assert(screen_attach_alternate(&s, alternate, 128) == 0);
    write_text("pppp\r\nqqqq\r\nrrrr\r\nss");
    write_text("\033[?1049hAAAA\r\nBBBB\r\nCCCC\r\nDD");
    assert(s.alternate_active && s.cursor_row == 3);
    assert(screen_resize(&s, 2, 4) == 0);
    row(0, "CCCC"); row(1, "DD  ");
    assert(s.cursor_row == 1 && s.history_count == 0);
    write_text("\033[?1049l");
    row(0, "rrrr"); row(1, "ss  ");
    assert(s.cursor_row == 1 && s.cursor_column == 2);
    assert(s.history_count == 2);
    line(0, "pppp"); line(1, "qqqq");
}

/* DEC 7: with autowrap off the tail of a long line stays on its row, the
 * last column taking each further glyph; turning it on wraps again. */
static void autowrap_mode(void)
{
    init(3, 4);
    write_text("\033[?7labcdef");
    row(0, "abcf");
    row(1, "    ");
    assert(s.cursor_row == 0 && s.cursor_column == 3 && !s.wrap_pending);
    write_text("\r\n\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad");
    assert(screen_cell(&s, 1, 2) == 0x4e2d && s.cursor_row == 1);
    valid_pairs();
    write_text("\033[?7h\r\nwxyzq");
    row(1, "wxyz");
    row(2, "q   ");
}
int main(void)
{
    underline_style();
    autowrap_mode();
    scrolling_region();
    explicit_region_scroll();
    reverse_index();
    insert_lines();
    delete_lines();
    resize_keeps_cursor_row();
    {
        const uint32_t wide[] = { 0x1100, 0x3000, 0x4e2d, 0xac00, 0xff01,
                                  0x1f600, 0x1f680, 0x1f1f5, 0x20000, 0x3fffd };
        const uint32_t narrow[] = { 'A', 0xa1, 0x105, 0x301, 0x200d, 0x20ac,
                                    0x2764, 0xfe0f, 0xff66, 0x10000, 0x40000, 0x10ffff };
        unsigned i;
        for (i = 0; i < sizeof wide / sizeof wide[0]; ++i)
            assert(screen_scalar_width(wide[i]) == 2);
        for (i = 0; i < sizeof narrow / sizeof narrow[0]; ++i)
            assert(screen_scalar_width(narrow[i]) == 1);
    }
    /* Code points must survive arbitrary byte boundaries without truncation. */
    {
        const unsigned char utf8[] = "Z\xc4\x85\xe2\x82\xac\xf0\x9f\x98\x80";
        size_t split;
        unsigned expected[] = { 'Z', 0x105, 0x20ac, 0x1f600 };
        for (split = 0; split < sizeof utf8; ++split) {
            init(2, 8);
            screen_write(&s, utf8, split);
            screen_write(&s, utf8 + split, sizeof utf8 - 1 - split);
            unsigned column;
            for (column = 0; column < 4; ++column)
                assert(screen_view_cell(&s, 0, column).ch == expected[column]);
            assert(s.cursor_column == 5); /* Emoji occupies two columns. */
        }
    }

    /* Wide output, positioning into either half, and overlapping writes. */
    init(2, 8);
    write_text("A中B");
    assert(s.cursor_column == 4 && screen_cell(&s, 0, 3) == 'B');
    write_text("\033[1;3HX");
    assert(screen_cell(&s, 0, 1) == ' ' && screen_cell(&s, 0, 2) == 'X');
    init(2, 8);
    write_text("中中\033[1;2H😀");
    assert(screen_cell(&s, 0, 0) == ' ' && screen_cell(&s, 0, 3) == ' ');
    assert(screen_cell(&s, 0, 1) == 0x1f600);
    valid_pairs();
    write_text("\033[1;2HQ");
    row(0, " Q      ");
    valid_pairs();

    /* Explicit cursor movement counts columns, including continuations. */
    init(2, 8);
    write_text("中\bX");
    row(0, " X      ");
    init(2, 8);
    write_text("中\033[1DX");
    row(0, " X      ");
    init(2, 8);
    write_text("中\r\033[1CX");
    row(0, " X      ");

    /* ED extends either boundary to cover an intersected pair. */
    init(2, 8);
    write_text("A中B\033[1;3H\033[J");
    row(0, "A       ");
    init(2, 8);
    write_text("A中B\033[1;2H\033[1J");
    row(0, "   B    ");
    valid_pairs();

    /* Erase either half, not just the addressed cell. */
    init(2, 8);
    write_text("A中B\033[1;3H\033[K");
    row(0, "A       ");
    init(2, 8);
    write_text("A中B\033[1;2H\033[1K");
    row(0, "   B    ");

    /* Wrap before a wide glyph when only the last column is free. */
    init(2, 4);
    write_text("abc中");
    row(0, "abc ");
    assert(screen_cell(&s, 1, 0) == 0x4e2d && s.cursor_column == 2);
    init(2, 4);
    write_text("ab中");
    assert(s.wrap_pending && s.cursor_row == 0 && s.cursor_column == 3);
    write_text("X");
    row(1, "X   ");
    init(2, 1);
    write_text("中");
    assert(screen_cell(&s, 0, 0) == 0xfffd && s.wrap_pending);

    /* Cropping a continuation must not leave half of a glyph. */
    init(2, 4);
    write_text("ab中");
    assert(screen_resize(&s, 2, 3) == 0);
    row(0, "ab ");
    assert(screen_resize(&s, 2, 4) == 0);
    row(0, "ab  ");

    /* Wrapping at an existing continuation clears the old pair first. */
    init(2, 4);
    write_text("ab中\033[1;4H😀");
    row(0, "ab  ");
    assert(screen_cell(&s, 1, 0) == 0x1f600);
    valid_pairs();

    /* SGR cannot split a pair; scroll/history retain both cells and styles. */
    init(2, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("\033[31;44m中\033[0m\r\n😀\r\nZ");
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 0).width == 2);
    assert(screen_view_cell(&s, 0, 1).fg == 1 && screen_view_cell(&s, 0, 1).bg == 4);
    valid_pairs();
    screen_scroll_view(&s, -1);
    assert(screen_cell(&s, 0, 0) == 0x1f600);
    assert(screen_resize(&s, 3, 4) == 0);
    valid_pairs();
    assert(s.history_count == 1);
    assert(screen_resize(&s, 3, 1) == 0);
    assert(s.history_count == 0 && screen_cell(&s, 0, 0) == ' ');
    valid_pairs();

    /* Invalid prefixes cannot consume a following glyph or escape command. */
    init(2, 16);
    write_text("\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80");
    {
        unsigned i;
        for (i = 0; i < 9; ++i) assert(screen_cell(&s, 0, i) == 0xfffd);
        assert(s.cursor_column == 9);
    }
    init(2, 8);
    write_text("\xe2\x82");
    assert(s.cursor_column == 0);
    write_text("\033[31mX");
    assert(screen_cell(&s, 0, 0) == 0xfffd);
    assert(screen_view_cell(&s, 0, 1).ch == 'X');
    assert(screen_view_cell(&s, 0, 1).fg == 1);
    init(2, 8);
    write_text("\xe2\xc4\x85");
    assert(screen_cell(&s, 0, 0) == 0xfffd);
    assert(screen_cell(&s, 0, 1) == 0x105);

    /* A multibyte glyph occupies the last column once, including after resize. */
    init(2, 2);
    assert(screen_attach_history(&s, history, 4) == 0);
    write_text("a\xc4\x85");
    assert(s.wrap_pending && s.cursor_row == 0);
    write_text("\xe2");
    assert(s.cursor_row == 0);
    assert(screen_resize(&s, 2, 3) == 0);
    write_text("\x82\xac");
    assert(screen_cell(&s, 0, 1) == 0x20ac);
    write_text("\r\nb\r\nc");
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 1).ch == 0x20ac);
    init(2, 8);
    write_text("\xe2");
    init(2, 8);
    write_text("A");
    assert(screen_cell(&s, 0, 0) == 'A');

    init(3, 8);
    write_text("one\rX\r\ntwo");
    row(0, "Xne     "); row(1, "two     ");
    assert(s.cursor_row == 1 && s.cursor_column == 3);
    init(3, 8);
    write_text("ab\nX\bY\r\bZ");
    row(1, "Z Y     ");
    write_text("\f");
    row(0, "        "); row(1, "        "); row(2, "        ");
    assert(s.cursor_row == 0 && s.cursor_column == 0);

    /* Delayed wrap: the final column stays visible until the next glyph. */
    init(2, 4);
    write_text("abcd");
    row(0, "abcd");
    assert(s.cursor_row == 0 && s.cursor_column == 3);
    write_text("e"); row(1, "e   ");
    write_text("\r\nZ");
    row(0, "e   "); row(1, "Z   ");

    /* CSI state survives every possible split in a stream. */
    {
        const char *stream = "abcd\033[2;3H\033[1;31;44mR\033[0m";
        size_t split;
        for (split = 0; split <= strlen(stream); ++split) {
            init(3, 8);
            screen_write(&s, (const unsigned char *)stream, split);
            screen_write(&s, (const unsigned char *)stream + split,
                         strlen(stream) - split);
            row(0, "abcd    "); row(1, "  R     ");
            assert(screen_view_cell(&s, 1, 2).fg == 1);
            assert(screen_view_cell(&s, 1, 2).bg == 4);
            assert(screen_view_cell(&s, 1, 2).bold == 1);
            assert(s.style.fg == 7 && s.style.bg == 0 && !s.style.bold);
        }
    }

    init(3, 8);
    write_text("abcdefgh\033[1;4H\033[K");
    row(0, "abc     ");
    write_text("\033[2;1Hsecond\033[1;3H\033[J");
    row(0, "ab      "); row(1, "        ");
    write_text("\033[2J");
    row(0, "        ");
    assert(s.cursor_row == 0 && s.cursor_column == 2);
    write_text("\033[0;0HX\033[999999999999999999;999999999999999999HZ");
    row(0, "X       "); row(2, "       Z");

    /* Unknown/oversized CSI and OSC consume payload without displaying it. */
    init(2, 8);
    write_text("\033[?1049h\033]0;hidden\007OK");
    row(0, "OK      ");
    write_text("\033[1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16;17m!");
    row(0, "OK!     ");
    assert(s.style.fg == 7 && !s.style.bold);

    /* A bounded ring retains the newest two lines, independently of display. */
    init(2, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("a\r\nb\r\nc\r\nd\r\ne");
    assert(s.history_count == 2);
    row(0, "d   "); row(1, "e   ");
    screen_scroll_view(&s, 99);
    assert(screen_view_cell(&s, 0, 0).ch == 'b');
    assert(screen_view_cell(&s, 1, 0).ch == 'c');
    screen_scroll_view(&s, -99);
    assert(screen_view_cell(&s, 0, 0).ch == 'd');

    /* Resize preserves left cells/styles, blanks exposed area, clamps the
     * cursor column and keeps the cursor row when the height shrinks. */
    init(2, 4);
    write_text("abcd\r\nef");
    assert(screen_resize(&s, 3, 6) == 0);
    row(0, "abcd  "); row(1, "ef    "); row(2, "      ");
    assert(screen_resize(&s, 1, 2) == 0);
    row(0, "ef");
    assert(s.cursor_row == 0 && s.cursor_column == 1);
    assert(screen_resize(&s, 100, 100) == -1);
    assert(s.rows == 1 && s.columns == 2);
    assert(screen_resize(&s, 0, 8) == -1);

    /* Width changes discard incompatible history and leave a safe viewport. */
    init(2, 4);
    assert(screen_attach_history(&s, history, 8) == 0);
    write_text("a\r\nb\r\nc");
    screen_scroll_view(&s, 1);
    assert(screen_resize(&s, 2, 6) == 0);
    assert(!s.history_count && !s.view_offset);
    write_text("\r\nx\r\ny");
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 0).ch == 'c');

    /* Repeat the same parser work across chunk boundaries and random resizes. */
    {
        unsigned seed = 19, step, r, c;
        init(3, 8);
        assert(screen_attach_history(&s, history, 32) == 0);
        for (step = 0; step < 10000; ++step) {
            unsigned char byte;
            seed = seed * 1664525u + 1013904223u;
            byte = (unsigned char)(seed >> 24);
            screen_write(&s, &byte, 1);
            if (step % 17 == 0) write_text("\033[H中😀\bX");
            if (step % 13 == 0)
                assert(screen_resize(&s, 1 + seed % 8, 1 + (seed >> 8) % 16) == 0);
            screen_scroll_view(&s, step % 2 ? -1 : 1);
            valid_pairs();
            assert(s.style.width == 1);
            assert(s.cursor_row < s.rows && s.cursor_column < s.columns);
            assert(s.history_count <= s.history_lines);
            assert(s.view_offset <= s.history_count);
            for (r = 0; r < s.rows; ++r)
                for (c = 0; c < s.columns; ++c) {
                    XTermCell cell = screen_view_cell(&s, r, c);
                    assert(cell.fg < 8 && cell.bg < 8 && cell.bold < 2);
                }
        }
    }

    assert(screen_init(NULL, cells, 128, 3, 8) == -1);
    assert(screen_init(&s, NULL, 128, 3, 8) == -1);
    assert(screen_init(&s, cells, 128, 0, 8) == -1);
    assert(screen_init(&s, cells, 128, 3, 0) == -1);
    assert(screen_init(&s, cells, 23, 3, 8) == -1);
    puts("screen model tests passed (UTF-8, wide pairs, CSI, SGR, erase, wrap, history, resize)");
    return 0;
}
