#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermScreen s;
static XTermCell cells[256], history[128];
static void init(unsigned rows, unsigned columns)
{
    assert(screen_init(&s, cells, 256, rows, columns) == 0);
}
static void write_text(const char *text)
{
    screen_write(&s, (const unsigned char *)text, strlen(text));
}

int main(void)
{
    static const struct { const char *text; unsigned width, length; uint32_t scalar[16]; } cases[] = {
        {"é", 1, 2, {'e', 0x301}},
        {"👨‍👩‍👧‍👦", 2, 7, {0x1f468,0x200d,0x1f469,0x200d,0x1f467,0x200d,0x1f466}},
        {"🇵🇱", 2, 2, {0x1f1f5,0x1f1f1}},
        {"❤️", 2, 2, {0x2764,0xfe0f}},
        {"1️⃣", 2, 3, {'1',0xfe0f,0x20e3}},
        {"👍🏽", 2, 2, {0x1f44d,0x1f3fd}},
        {"क्‍ष", 1, 4, {0x915,0x94d,0x200d,0x937}},
        {"❤‍❤", 2, 3, {0x2764,0x200d,0x2764}},
        {"́̂", 1, 2, {0x301,0x302}},
        {"각", 2, 3, {0x1100,0x1161,0x11a8}},
        {"e️", 1, 2, {'e',0xfe0f}}
    };
    size_t i, split;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        size_t length = strlen(cases[i].text);
        for (split = 0; split <= length; ++split) {
            init(2, 16);
            screen_write(&s, (const unsigned char *)cases[i].text, split);
            screen_write(&s, (const unsigned char *)cases[i].text + split, length - split);
            if (s.cursor_column != cases[i].width) {
                fprintf(stderr, "cluster %lu split %lu: expected cursor %u, got %u\n",
                        (unsigned long)i, (unsigned long)split, cases[i].width, s.cursor_column);
                return 1;
            }
            assert(cells[0].length == cases[i].length);
            assert(cells[0].ch == cases[i].scalar[0]);
            assert(!memcmp(cells[0].suffix, cases[i].scalar + 1,
                           (cases[i].length - 1) * sizeof(uint32_t)));
            write_text("X");
            assert(cells[cases[i].width].ch == 'X');
        }
        init(2, 16);
        for (split = 0; split < length; ++split)
            screen_write(&s, (const unsigned char *)cases[i].text + split, 1);
        assert(cells[0].length == cases[i].length && s.cursor_column == cases[i].width);
    }
    init(2, 4);
    assert(screen_attach_history(&s, history, 128) == 0);
    write_text("abc❤️");
    assert(screen_cell(&s, 0, 3) == ' ');
    assert(screen_cell(&s, 1, 0) == 0x2764 && cells[4].width == 2);
    assert(cells[4].length == 2 && cells[4].suffix[0] == 0xfe0f);
    init(2, 4);
    write_text("ab❤️");
    assert(s.wrap_pending && cells[2].length == 2 && cells[3].length == 0);
    write_text("X");
    assert(cells[4].ch == 'X');
    init(2, 4);
    write_text("abcé");
    assert(s.wrap_pending && cells[3].length == 2 && cells[4].length == 0);
    /* Width promotion on the bottom row scrolls once and keeps the suffix. */
    init(1, 4);
    assert(screen_attach_history(&s, history, 128) == 0);
    write_text("abc1️⃣");
    assert(cells[0].ch == '1' && cells[0].length == 3 && s.history_count == 1);
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 3).length == 0);
    screen_scroll_view(&s, -1);
    write_text("\033[1;2HX");
    assert(cells[0].length == 0 && cells[1].ch == 'X');
    /* Regional indicator parity restarts at each pair. */
    init(2, 16);
    write_text("🇵🇱🇬🇧🇺");
    assert(s.cursor_column == 6 && cells[0].length == 2 && cells[2].length == 2);
    assert(cells[4].length == 1);
    /* An intervening control, even SGR/BEL/C1, ends attachment. */
    init(2, 16);
    write_text("e\033[31ḿe\áe\xc2\x85́");
    assert(s.cursor_column == 6 && cells[0].length == 1 && cells[1].ch == 0x301);
    /* Erase, history and resize copy/drop full clusters. */
    init(2, 4);
    assert(screen_attach_history(&s, history, 128) == 0);
    write_text("é\r\n❤️\r\nZ");
    screen_scroll_view(&s, 1);
    assert(screen_view_cell(&s, 0, 0).suffix[0] == 0x301);
    screen_scroll_view(&s, -1);
    assert(screen_resize(&s, 3, 4) == 0 && cells[0].suffix[0] == 0xfe0f);
    assert(screen_resize(&s, 3, 1) == 0 && cells[0].length == 0);
    init(2, 8);
    write_text("❤️\033[1;2H\033[K");
    assert(cells[0].length == 0 && cells[1].length == 0);
    /* Overflow replaces the excess scalar, then starts a fresh boundary. */
    init(2, 8);
    write_text("e");
    for (i = 0; i < 15; ++i) write_text("́");
    assert(cells[0].length == 16 && s.cursor_column == 1);
    write_text("́́X");
    assert(cells[0].length == 16 && cells[1].ch == 0xfffd && cells[1].length == 1);
    assert(cells[2].ch == 0x301 && cells[3].ch == 'X');
    /* A one-column viewport emits a replacement for a promoted cluster. */
    init(2, 1);
    write_text("❤️");
    assert(cells[0].ch == 0xfffd && cells[0].length == 1 && cells[0].width == 1);
    puts("grapheme model tests passed");
    return 0;
}
