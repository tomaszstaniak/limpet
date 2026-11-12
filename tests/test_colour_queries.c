/* Colour queries: a program asks what the default colours and the palette
 * are (OSC 10, 11, 12, 4) or whether the terminal is dark or light
 * (CSI ? 996 n, and reports with mode 2031). Setting colours is not
 * offered: those sequences are consumed without an answer. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermScreen s;
static XTermCell cells[80*4];
static char got[512];
static size_t got_length;
static void sink(void *context,const unsigned char *bytes,size_t n)
{
    (void)context;
    assert(got_length+n<sizeof got);
    memcpy(got+got_length,bytes,n); got_length+=n; got[got_length]=0;
}
static const uint32_t palette[16]={0x000000,0xcc0000,0x00cc00,0xcccc00,0x0000cc,0xcc00cc,0x00cccc,0xcccccc,
                                   0x555555,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff};
static void fresh(int colours)
{
    assert(!screen_init(&s,cells,80*4,4,80));
    s.reply=sink;
    if(colours) screen_set_colours(&s,palette,0x12abef,0x101820,0xffcc00);
    got_length=0; got[0]=0;
}
static void put(const char *text) { screen_write(&s,(const unsigned char *)text,strlen(text)); }
/* The query answers the same at every split into two writes. */
static void expect(const char *query,const char *answer)
{
    size_t n=strlen(query),split;
    for(split=0;split<=n;++split) {
        fresh(1);
        screen_write(&s,(const unsigned char *)query,split);
        screen_write(&s,(const unsigned char *)query+split,n-split);
        if(strcmp(got,answer)) {
            printf("query %zu bytes, split %zu: got %zu bytes, expected %zu\n",n,split,got_length,strlen(answer));
            assert(0);
        }
        /* Nothing of a query is drawn. */
        assert(s.cursor_column==0 && s.cursor_row==0);
    }
}

int main(void)
{
    /* Default foreground, background and cursor; the answer ends as the query did. */
    expect("\033]10;?\033\\","\033]10;rgb:1212/abab/efef\033\\");
    expect("\033]10;?\007","\033]10;rgb:1212/abab/efef\007");
    expect("\033]11;?\033\\","\033]11;rgb:1010/1818/2020\033\\");
    expect("\033]11;?\007","\033]11;rgb:1010/1818/2020\007");
    expect("\033]12;?\007","\033]12;rgb:ffff/cccc/0000\007");
    /* Palette entries 0..15. */
    expect("\033]4;1;?\007","\033]4;1;rgb:cccc/0000/0000\007");
    expect("\033]4;15;?\033\\","\033]4;15;rgb:ffff/ffff/ffff\033\\");
    expect("\033]4;0;?\007","\033]4;0;rgb:0000/0000/0000\007");
    /* Not offered or not a query: consumed, no answer, nothing drawn. */
    expect("\033]4;16;?\007","");
    expect("\033]4;256;?\007","");
    expect("\033]4;1;rgb:ff/00/00\007","");
    expect("\033]10;#ffffff\007","");
    expect("\033]11;rgb:00/00/00\033\\","");
    expect("\033]10;?;?\007","");
    expect("\033]13;?\007","");
    expect("\033]1;?\007","");
    expect("\033]104\007","");
    expect("\033]110\007","");
    expect("\033]4;?\007","");
    expect("\033]4;1;?x\007","");
    expect("\033]10;?\030","");                /* CAN aborts the string */
    /* A title still works beside the queries. */
    fresh(1);
    put("\033]2;hello\007\033]11;?\007");
    assert(!strcmp(s.title,"hello") && !strcmp(got,"\033]11;rgb:1010/1818/2020\007"));
    /* Dark or light: CSI ? 996 n. */
    expect("\033[?996n","\033[?997;1n");
    fresh(0);
    screen_set_colours(&s,palette,0x202020,0xf5f5f0,0x202020);
    put("\033[?996n");
    assert(!strcmp(got,"\033[?997;2n"));
    /* Before the frontend has said what the colours are: no answer at all. */
    fresh(0);
    put("\033]11;?\007\033[?996n");
    assert(!got_length);
    /* No sink: nothing happens, nothing crashes. */
    assert(!screen_init(&s,cells,80*4,4,80));
    screen_set_colours(&s,palette,0xffffff,0x000000,0xffffff);
    put("\033]11;?\007\033[?996n");
    /* Mode 2031: a report when the theme goes from dark to light or back,
       not for a change that stays on one side, and not without the mode. */
    fresh(1);
    screen_set_colours(&s,palette,0x202020,0xf5f5f0,0x202020);
    assert(!got_length);
    fresh(1);
    put("\033[?2031h");
    assert(!got_length);
    screen_set_colours(&s,palette,0x202020,0xf5f5f0,0x202020);
    assert(!strcmp(got,"\033[?997;2n"));
    got_length=0; got[0]=0;
    screen_set_colours(&s,palette,0x000000,0xffffff,0x000000);
    assert(!got_length);
    screen_set_colours(&s,palette,0xffffff,0x000000,0xffffff);
    assert(!strcmp(got,"\033[?997;1n"));
    got_length=0; got[0]=0;
    put("\033[?2031l");
    screen_set_colours(&s,palette,0x202020,0xf5f5f0,0x202020);
    assert(!got_length);
    puts("colour queries PASS: OSC 10/11/12/4 at every split, both terminators, settings ignored, dark/light report, mode 2031");
    return 0;
}
