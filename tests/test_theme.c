#include "../src/theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[80], alt[80], history[80];
static void emit(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
static void true_colours(void)
{
    XTheme t;
    XTermCell cell;
    const char *seq="\033[38;2;12;34;56;48;2;201;202;203;1mX";
    size_t split;
    theme_default(&t);
    for(split=0;split<=strlen(seq);++split) {
        assert(!screen_init(&s,cells,80,2,40));
        screen_write(&s,(const unsigned char *)seq,split);
        emit(seq+split);
        assert(theme_fg(&t,&cells[0])==0x0c2238);
        assert(theme_bg(&t,&cells[0])==0xc9cacb);
        assert(cells[0].bold);
    }
    assert(!screen_init(&s,cells,80,2,40));
    assert(!screen_attach_history(&s,history,80));
    assert(!screen_attach_alternate(&s,alt,80));
    emit("\033[38;2;1;2;3;48;2;254;253;252mA\033[?1049h");
    emit("\033[38;2;9;8;7mX\033[?1049lB");
    assert(theme_fg(&t,&cells[1])==0x010203);
    assert(theme_bg(&t,&cells[1])==0xfefdfc);
    emit("\033[39;49mC\033[38;5;196;48;5;21mD");
    assert(theme_fg(&t,&cells[2])==t.foreground);
    assert(theme_bg(&t,&cells[2])==t.background);
    assert(theme_fg(&t,&cells[3])==0xff0000);
    assert(theme_bg(&t,&cells[3])==0x0000ff);
    emit("\033[38;2;17;34;51;48;2;68;85;102mE\r\nF\r\nG");
    cell=screen_line_cell(&s,0,4);
    assert(theme_fg(&t,&cell)==0x112233);
    assert(theme_bg(&t,&cell)==0x445566);
    assert(!screen_resize(&s,2,30));
    cell=screen_view_cell(&s,0,0);
    assert(theme_fg(&t,&cell)==0x112233);
    cell=screen_view_cell(&s,1,0);
    assert(theme_bg(&t,&cell)==0x445566);
    assert(!screen_init(&s,cells,80,2,40));
    emit("\033[31;44m\033[38;2;256;1;2;1mA\033[48;2;1;2mB\033[38;2;4;5;6mC");
    assert(theme_fg(&t,&cells[0])==t.ansi[1] && !cells[0].bold);
    assert(theme_bg(&t,&cells[1])==t.ansi[4]);
    assert(theme_fg(&t,&cells[2])==0x040506);
    emit("\033[38;5;7mD\033[48;5;8mE\033[0mF");
    assert(theme_fg(&t,&cells[3])==t.ansi[7]);
    assert(theme_bg(&t,&cells[4])==t.ansi[8]);
    assert(theme_fg(&t,&cells[5])==t.foreground);
    assert(theme_bg(&t,&cells[5])==t.background);
    emit("\033[38;2;17;34;51;48;2;68;85;102mH");
    cells[6].colour_flags|=XCOLOUR_SELECTED;
    assert(theme_fg(&t,&cells[6])==t.selection_fg);
    assert(theme_bg(&t,&cells[6])==t.selection_bg);
    puts("truecolor PASS: fragmented RGB, mixed styles, invalid payload, reset, history, alternate, resize, selection");
}
/* Catches swallowed extended SGR, wrong cube/grayscale boundaries and
 * payload bytes accidentally interpreted as independent attributes. */
static void indexed_colours(void)
{
    static const struct { unsigned index; uint32_t rgb; } cases[]={
        {16,0x000000},{17,0x00005f},{21,0x0000ff},{22,0x005f00},
        {51,0x00ffff},{52,0x5f0000},{67,0x5f87af},{196,0xff0000},
        {231,0xffffff},{232,0x080808},{243,0x767676},{255,0xeeeeee}};
    XTheme t;
    unsigned i;
    char seq[80];
    theme_default(&t);
    for(i=0;i<sizeof cases/sizeof cases[0];++i) {
        assert(!screen_init(&s,cells,80,2,40));
        snprintf(seq,sizeof seq,"\033[38;5;%u;48;5;%umX",cases[i].index,cases[i].index);
        emit(seq);
        assert(theme_fg(&t,&cells[0])==cases[i].rgb);
        assert(theme_bg(&t,&cells[0])==cases[i].rgb);
    }
    for(i=0;i<256;++i) {
        size_t split, length;
        snprintf(seq,sizeof seq,"\033[38;5;%u;48;5;%u;1mX",i,255-i);
        length=strlen(seq);
        for(split=0;split<=length;++split) {
            assert(!screen_init(&s,cells,80,2,40));
            screen_write(&s,(const unsigned char *)seq,split);
            emit(seq+split);
            assert(cells[0].fg==i && cells[0].bg==255-i && cells[0].bold);
            assert(!(cells[0].colour_flags&(XCOLOUR_DEFAULT_FG|XCOLOUR_DEFAULT_BG)));
        }
    }
    assert(!screen_init(&s,cells,80,2,40));
    assert(!screen_attach_alternate(&s,alt,80));
    emit("\033[38;5;67;48;5;243mA\033[?1049h\033[31mZ\033[?1049lB");
    assert(theme_fg(&t,&cells[1])==0x5f87af && theme_bg(&t,&cells[1])==0x767676);
    emit("\033[39;49mD\033[38;5;196mR\033[0mN");
    assert(theme_fg(&t,&cells[2])==t.foreground && theme_bg(&t,&cells[2])==t.background);
    assert(theme_fg(&t,&cells[4])==t.foreground);
    emit("\033[31;44m\033[38;5;999;1mI\033[48;5mJ\033[38;2;1;0;0mK");
    for(i=5;i<7;++i) {
        assert(theme_fg(&t,&cells[i])==t.ansi[1]);
        assert(theme_bg(&t,&cells[i])==t.ansi[4]);
        assert(!cells[i].bold);
    }
    assert(theme_fg(&t,&cells[7])==0x010000);
    assert(theme_bg(&t,&cells[7])==t.ansi[4]);
    emit("\033[38;5;1;48;5;2mT\033[1mB");
    t.ansi[1]=0x123456; t.ansi[2]=0xabcdef;
    assert(theme_fg(&t,&cells[8])==0x123456 && theme_bg(&t,&cells[8])==0xabcdef);
    assert(theme_fg(&t,&cells[9])==t.ansi[9]); /* Existing bold-bright policy. */
    cells[8].colour_flags|=XCOLOUR_SELECTED;
    assert(theme_fg(&t,&cells[8])==t.selection_fg && theme_bg(&t,&cells[8])==t.selection_bg);
    puts("indexed colour PASS: palette boundaries, all indices/splits, reset, alternate, rejection, theme");
}
/* Inverse swaps resolved colours, not raw palette indices. This catches
 * ignoring SGR 7/27, losing inverse across colour changes, and selecting
 * inverse colours instead of the configured selection pair. */
static void inverse_colours(void)
{
    XTheme t;
    XTermCell cell;
    size_t split;
    const char *seq="\033[38;2;18;52;86;48;5;196;7mX";
    theme_default(&t);
    t.foreground=0x123456; t.background=0xabcdef;
    for(split=0;split<=strlen(seq);++split) {
        assert(!screen_init(&s,cells,80,2,40));
        screen_write(&s,(const unsigned char *)seq,split);
        emit(seq+split);
        assert(theme_fg(&t,&cells[0])==0xff0000);
        assert(theme_bg(&t,&cells[0])==0x123456);
    }
    emit("\033[27mN\033[7mI\033[39;49mD\033[0mR");
    assert(theme_fg(&t,&cells[1])==0x123456);
    assert(theme_bg(&t,&cells[1])==0xff0000);
    assert(theme_fg(&t,&cells[2])==0xff0000);
    assert(theme_bg(&t,&cells[2])==0x123456);
    assert(theme_fg(&t,&cells[3])==0xabcdef);
    assert(theme_bg(&t,&cells[3])==0x123456);
    assert(theme_fg(&t,&cells[4])==0x123456);
    assert(theme_bg(&t,&cells[4])==0xabcdef);
    cells[3].colour_flags|=XCOLOUR_SELECTED;
    assert(theme_fg(&t,&cells[3])==t.selection_fg);
    assert(theme_bg(&t,&cells[3])==t.selection_bg);
    assert(!screen_init(&s,cells,80,2,40));
    assert(!screen_attach_history(&s,history,80));
    assert(!screen_attach_alternate(&s,alt,80));
    emit("\033[7mA\033[?1049h\033[27mB\033[?1049lC");
    assert(theme_fg(&t,&cells[0])==0xabcdef);
    assert(theme_bg(&t,&cells[0])==0x123456);
    assert(theme_fg(&t,&cells[1])==0xabcdef);
    assert(theme_bg(&t,&cells[1])==0x123456);
    emit("\r\n\r\n\r\n");
    cell=screen_line_cell(&s,0,0);
    assert(theme_fg(&t,&cell)==0xabcdef);
    assert(theme_bg(&t,&cell)==0x123456);
    assert(!screen_init(&s,cells,80,2,40));
    emit("\033[1;31;44;7mB\033[22;27mN");
    assert(theme_fg(&t,&cells[0])==t.ansi[4]);
    assert(theme_bg(&t,&cells[0])==t.ansi[9]);
    assert(theme_fg(&t,&cells[1])==t.ansi[1]);
    assert(theme_bg(&t,&cells[1])==t.ansi[4]);
    puts("inverse PASS: RGB/indexed/default, fragmented input, reset, selection, alternate, history");
}
/* AROS One's Shell prompt: "\e[42m\e[31m%N.\e[43m\e[32m%s\e[42m\e[31m>\e[40m\e[31m " */
static void amiga_pens(void)
{
    XTheme t;
    theme_default(&t);
    assert(!screen_init(&s,cells,80,2,40));
    s.amiga_pens=1;
    emit("\033[42m\033[31m9.\033[43m\033[32mA\033[42m\033[31m>\033[40m\033[31m x");
    assert(theme_fg(&t,&cells[0])==t.foreground && theme_bg(&t,&cells[0])==0x000000);
    assert(theme_fg(&t,&cells[2])==0x000000 && theme_bg(&t,&cells[2])==t.ansi[12]);
    assert(theme_fg(&t,&cells[3])==t.foreground && theme_bg(&t,&cells[3])==0x000000);
    /* after the prompt, typed text is pen 1 on pen 0: the theme's own colours */
    assert(theme_fg(&t,&cells[5])==t.foreground && theme_bg(&t,&cells[5])==t.background);
    /* a pen sticks to its own plane, and any other colour ends it */
    emit("\033[42m\033[38;5;2mg\033[91mr\033[31m\033[48;5;4mb");
    assert(theme_fg(&t,&cells[6])==t.ansi[2] && theme_bg(&t,&cells[6])==0x000000);
    assert(theme_fg(&t,&cells[7])==t.ansi[9]);
    assert(theme_fg(&t,&cells[8])==t.foreground && theme_bg(&t,&cells[8])==t.ansi[4]);
    /* a light theme keeps the Workbench look: black on white, white on blue */
    t.background=0xf0f0f0; t.foreground=0x101010;
    assert(theme_bg(&t,&cells[0])==0xffffff && theme_bg(&t,&cells[2])==t.ansi[4]);
    theme_default(&t);
    /* the op 13 PENS contract: 38;5;0..15 and 48;5;0..15 keep their ANSI
       meaning in pen mode */
    {
        unsigned n; char seq[40];
        for(n=0;n<16;++n) {
            assert(!screen_init(&s,cells,80,2,40)); s.amiga_pens=1;
            snprintf(seq,sizeof seq,"\033[42m\033[38;5;%um\033[48;5;%umq",n,n);
            emit(seq);
            assert(theme_fg(&t,&cells[0])==t.ansi[n] && theme_bg(&t,&cells[0])==t.ansi[n]);
        }
        /* 39 restores the default foreground only: background pen and bold stay */
        assert(!screen_init(&s,cells,80,2,40)); s.amiga_pens=1;
        emit("\033[1m\033[43m\033[32mv\033[39mw");
        assert(theme_fg(&t,&cells[1])==t.foreground && theme_bg(&t,&cells[1])==t.ansi[12] &&
               cells[1].bold);
        /* a verdict as apkg writes it, then AROS One's next prompt: the
           prompt's own pens, and typed text in the theme's colours */
        emit("\033[1m\033[38;5;2mOK\033[0m\r\n");
        emit("\033[42m\033[31m9.\033[43m\033[32mA\033[42m\033[31m>\033[40m\033[31m x");
        assert(theme_fg(&t,&cells[40])==t.foreground && theme_bg(&t,&cells[40])==0x000000);
        assert(theme_fg(&t,&cells[42])==0x000000 && theme_bg(&t,&cells[42])==t.ansi[12]);
        assert(theme_fg(&t,&cells[45])==t.foreground && theme_bg(&t,&cells[45])==t.background &&
               !cells[45].bold);
    }
    /* without pen mode the same bytes are ANSI colours */
    assert(!screen_init(&s,cells,80,2,40));
    emit("\033[42m\033[31m9");
    assert(theme_fg(&t,&cells[0])==t.ansi[1] && theme_bg(&t,&cells[0])==t.ansi[2]);
}
int main(void)
{
    XTheme t, saved;
    true_colours();
    amiga_pens();
    indexed_colours();
    inverse_colours();
    theme_default(&t);
    t.foreground=0x123456; t.background=0xabcdef;
    assert(!screen_init(&s,cells,80,2,40));
    assert(!screen_attach_alternate(&s,alt,80));
    emit("a\033[37;40mb\033[39;49mc");
    assert(theme_fg(&t,&cells[0])==0x123456);
    assert(theme_bg(&t,&cells[0])==0xabcdef);
    assert(theme_fg(&t,&cells[1])==t.ansi[7]);
    assert(theme_bg(&t,&cells[1])==t.ansi[0]);
    assert(theme_fg(&t,&cells[2])==0x123456);
    assert(theme_bg(&t,&cells[2])==0xabcdef);
    emit("\033[?1049h\033[31mR\033[?1049l\033[1mD");
    assert(theme_fg(&t,&cells[3])==0x123456);
    emit("\033[31mR");
    assert(theme_fg(&t,&cells[4])==t.ansi[9]);
    cells[4].colour_flags=XCOLOUR_SELECTED;
    assert(theme_fg(&t,&cells[4])==t.selection_fg);
    assert(theme_bg(&t,&cells[4])==t.selection_bg);
    assert(!theme_parse(&t,"# comment\r\n\r\ncursor=#aAbBcC\r\n"));
    assert(t.cursor==0xaabbcc);
    assert(!theme_parse(&t,"foreground=#102030\nbackground=#f0f1f2\nansi1=#ff0000\n"));
    assert(t.foreground==0x102030 && t.background==0xf0f1f2 && t.ansi[1]==0xff0000);
    saved=t;
    assert(theme_parse(&t,"foreground=#000000\nunknown=#ffffff\n"));
    assert(!memcmp(&t,&saved,sizeof t));
    {
        char oversized[4098];
        memset(oversized,'\n',sizeof oversized);
        oversized[4097]=0;
        assert(theme_parse(&t,oversized));
        assert(!memcmp(&t,&saved,sizeof t));
        oversized[4096]=0;
        assert(!theme_parse(&t,oversized));
        memset(oversized,'x',96); oversized[96]=0;
        assert(theme_parse(&t,oversized));
    }
    assert(theme_parse(&t,"ansi16=#ffffff\n"));
    assert(theme_parse(&t,"foreground=#12345g\n"));
    assert(theme_parse(&t,"foreground=#ffffffjunk\n"));
    assert(theme_parse(&t,"foreground=#ffffff\nforeground=#000000\n"));
    assert(theme_load(&t,"/no-such-xterm-theme"));
    assert(!memcmp(&t,&saved,sizeof t));
    assert(!theme_load(&t,"themes/dark.theme"));
    assert(!theme_load(&t,"themes/light.theme"));
    assert(t.foreground!=t.ansi[7] && t.background!=t.ansi[0]);
    assert(!theme_load(&t,"themes/high-contrast.theme"));
    {
        unsigned i;
        char seq[48];
        assert(!screen_init(&s,cells,80,2,40));
        for(i=0;i<8;++i) {
            snprintf(seq,sizeof seq,"\033[%u;%umX",90+i,100+i);
            emit(seq);
            assert(cells[i].fg==8+i && cells[i].bg==8+i);
            assert(!(cells[i].colour_flags&(XCOLOUR_DEFAULT_FG|XCOLOUR_DEFAULT_BG)));
            assert(theme_fg(&t,&cells[i])==t.ansi[8+i]);
            assert(theme_bg(&t,&cells[i])==t.ansi[8+i]);
        }
        emit("\033[1;91;104mB\033[22mN\033[39;49mD\033[0mR");
        assert(theme_fg(&t,&cells[8])==t.ansi[9]);
        assert(theme_fg(&t,&cells[9])==t.ansi[9]);
        assert(theme_bg(&t,&cells[9])==t.ansi[12]);
        assert(theme_fg(&t,&cells[10])==t.foreground && theme_bg(&t,&cells[10])==t.background);
        assert(theme_fg(&t,&cells[11])==t.foreground && theme_bg(&t,&cells[11])==t.background);
    }
    puts("theme PASS: default/ANSI separation, reset, alternate, bold, atomic parsing, presets");
    return 0;
}
