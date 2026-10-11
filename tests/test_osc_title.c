/* OSC 0/2 window titles in the screen model: accepted only whole, valid and
 * bounded, whatever way the bytes are split between writes, and never shown
 * as text. */
#include "../src/screen_model.h"
#include "../src/hyperlink.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermCell cells[24 * 80];
static XHyperlinks links;

static void fresh(XTermScreen *s)
{
    assert(!screen_init(s, cells, 24 * 80, 24, 80));
    hyperlinks_init(&links);
    screen_attach_links(s, &links);
}

static void put(XTermScreen *s, const char *bytes)
{ screen_write(s, (const unsigned char *)bytes, strlen(bytes)); }

/* Feed the sequence split at every position and check the same outcome. */
static void every_split(const char *seq, int accepted, const char *expected)
{
    size_t n = strlen(seq), cut, a, b;
    for (cut = 0; cut <= n; ++cut)
        for (b = cut; b <= n; ++b) {
            XTermScreen s;
            fresh(&s);
            put(&s, "\033]2;before\a");
            a = s.title_serial;
            screen_write(&s, (const unsigned char *)seq, cut);
            screen_write(&s, (const unsigned char *)seq + cut, b - cut);
            screen_write(&s, (const unsigned char *)seq + b, n - b);
            put(&s, "X");
            assert(s.title_serial == a + (accepted ? 1 : 0));
            assert(!strcmp(s.title, accepted ? expected : "before"));
            /* Title bytes never reach the grid; the parser is back in text. */
            assert(screen_cell(&s, 0, 0) == 'X');
        }
}

int main(void)
{
    XTermScreen s;
    char big[XSCREEN_TITLE_MAX + 16];
    unsigned long serial;

    fresh(&s);
    assert(!s.title[0] && !s.title_serial);
    every_split("\033]2;Hardcode: replying\a", 1, "Hardcode: replying");
    every_split("\033]0;Hardcode: replying\033\\", 1, "Hardcode: replying");
    every_split("\033]2;Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 \xe6\xbc\xa2\xf0\x9f\x98\x80\033\\", 1,
                "Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 \xe6\xbc\xa2\xf0\x9f\x98\x80");
    /* Empty: the program drops its title. */
    every_split("\033]2;\a", 1, "");
    /* Not titles: OSC 1 (icon name), other commands, missing ';'. */
    every_split("\033]1;icon\a", 0, NULL);
    every_split("\033]21;x\a", 0, NULL);
    every_split("\033]2x\a", 0, NULL);
    every_split("\033]20;x\a", 0, NULL);
    /* Controls, DEL, C1 in UTF-8 form, malformed UTF-8: dropped whole. */
    every_split("\033]2;a\tb\a", 0, NULL);
    every_split("\033]2;a\x7f\a", 0, NULL);
    every_split("\033]2;a\xc2\x9b" "2J\a", 0, NULL);
    every_split("\033]2;a\xc3\a", 0, NULL);
    every_split("\033]2;a\xc0\xaf\a", 0, NULL);
    every_split("\033]2;a\xed\xa0\x80\a", 0, NULL);
    every_split("\033]2;a\xf4\x90\x80\x80\a", 0, NULL);
    /* ESC inside the text that is not ST ends nothing useful: dropped. */
    every_split("\033]2;a\033[31mb\a", 0, NULL);

    /* Exactly the limit is accepted; one byte more is dropped whole. */
    fresh(&s);
    memcpy(big, "\033]2;", 4);
    memset(big + 4, 'x', XSCREEN_TITLE_MAX);
    big[4 + XSCREEN_TITLE_MAX] = '\a';
    big[5 + XSCREEN_TITLE_MAX] = 0;
    put(&s, big);
    assert(s.title_serial == 1 && strlen(s.title) == XSCREEN_TITLE_MAX);
    big[4 + XSCREEN_TITLE_MAX] = 'x';
    big[5 + XSCREEN_TITLE_MAX] = '\a';
    big[6 + XSCREEN_TITLE_MAX] = 0;
    put(&s, big);
    assert(s.title_serial == 1 && strlen(s.title) == XSCREEN_TITLE_MAX);

    /* The same text again still counts: the frontend may have dropped it. */
    fresh(&s);
    put(&s, "\033]2;same\a");
    serial = s.title_serial;
    put(&s, "\033]2;same\a");
    assert(s.title_serial == serial + 1);
    /* Links keep working next to titles. */
    put(&s, "\033]8;;https://aros.org/\033\\L\033]8;;\033\\");
    assert(screen_cell(&s, 0, 0) == 'L' && s.cells[0].link_id);
    assert(!strcmp(s.title, "same"));
    puts("OSC TITLE PASS: OSC 0/2 at every split, BEL and ST, UTF-8, empty reset, limit, "
         "controls/C1/malformed/overlong dropped whole, other OSC ignored, links unaffected");
    return 0;
}
