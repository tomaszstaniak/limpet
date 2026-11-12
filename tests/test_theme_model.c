/* The theme model beyond parsing colours: roles a file did not give, the
 * Workbench pens under unusual palettes, pens of a theme's own, and the
 * bold switch. */
#include "../src/theme.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned distance(uint32_t a,uint32_t b)
{ int d=(int)theme_luma(a)-(int)theme_luma(b); return (unsigned)(d<0?-d:d); }

static void roles_derived(void)
{
    XTheme t;
    unsigned i;
    unsigned long seen=0;
    /* Only 0..7 and the two grounds: the bright half copies the normal one. */
    theme_default(&t);
    for(i=0;i<8;++i) { t.ansi[i]=0x101010*(i+1); seen|=1UL<<i; }
    t.foreground=0xeeeeee; t.background=0x111111; seen|=XROLE_FG|XROLE_BG;
    assert(!theme_complete(&t,seen));
    for(i=0;i<8;++i) assert(t.ansi[8+i]==t.ansi[i]);
    assert(t.cursor==0xeeeeee);
    /* No selection: inverse video. */
    assert(t.selection_bg==0xeeeeee && t.selection_fg==0x111111);
    /* A selection background alone: the text is the ground further from it. */
    theme_default(&t);
    t.foreground=0xeeeeee; t.background=0x111111; t.selection_bg=0x333333;
    assert(!theme_complete(&t,XROLE_FG|XROLE_BG|XROLE_SEL_BG|0xffff));
    assert(t.selection_fg==0xeeeeee);
    t.selection_bg=0xcccccc;
    assert(!theme_complete(&t,XROLE_FG|XROLE_BG|XROLE_SEL_BG|0xffff));
    assert(t.selection_fg==0x111111);
    /* Sixteen palette entries and no grounds: 7 on 0. */
    theme_default(&t);
    for(i=0;i<16;++i) t.ansi[i]=0x0f0f0f*i+0x010101;
    assert(!theme_complete(&t,0xffff));
    assert(t.foreground==t.ansi[7] && t.background==t.ansi[0]);
    /* Grounds only, light: the palette is a light one, legible on it. */
    theme_default(&t);
    t.foreground=0x202020; t.background=0xf0f0f0;
    assert(!theme_complete(&t,XROLE_FG|XROLE_BG));
    for(i=1;i<7;++i) assert(distance(t.ansi[i],t.background)>=40);
    /* Grounds only, dark: the built-in dark palette. */
    { XTheme dark; theme_default(&dark); theme_default(&t);
      t.foreground=0xdddddd; t.background=0x000000;
      assert(!theme_complete(&t,XROLE_FG|XROLE_BG));
      assert(!memcmp(t.ansi,dark.ansi,sizeof t.ansi)); }
    /* Neither grounds nor eight palette entries: not a theme. */
    theme_default(&t);
    assert(theme_complete(&t,0x7f)==-1);
    assert(theme_complete(&t,XROLE_FG)==-1);
    assert(theme_complete(&t,0)==-1);
    puts("theme roles PASS: bright half, cursor, selection, grounds, light and dark palettes, not a theme");
}

static void pens_guarded(void)
{
    XTheme t;
    unsigned i;
    /* The built-in theme keeps today's pens. */
    theme_default(&t);
    assert(theme_pen(&t,0)==t.background && theme_pen(&t,1)==t.foreground);
    assert(theme_pen(&t,2)==0x000000 && theme_pen(&t,3)==t.ansi[12]);
    assert(theme_pen(&t,4)==t.ansi[8] && theme_pen(&t,5)==t.ansi[3]);
    assert(theme_pen(&t,6)==t.ansi[14] && theme_pen(&t,7)==t.ansi[11]);
    /* Solarized-like: bright blue equals the foreground, bright black the
       background. The accent must not be the text colour, and pen 4 must
       be readable. */
    theme_default(&t);
    t.background=0x002b36; t.foreground=0x839496;
    t.ansi[12]=0x839496; t.ansi[4]=0x268bd2; t.ansi[8]=0x002b36;
    assert(theme_pen(&t,3)!=t.foreground);
    assert(distance(theme_pen(&t,3),theme_pen(&t,2))>=48);
    assert(theme_pen(&t,3)==0x268bd2);
    assert(theme_pen(&t,4)==t.foreground);
    /* Both blues unusable: the selection background stands in. */
    t.ansi[4]=0x839496; t.selection_bg=0x586e75;
    assert(theme_pen(&t,3)==0x586e75);
    /* Every pen 4..7 is at least 40 from the background, or the foreground. */
    for(i=4;i<8;++i)
        assert(theme_pen(&t,i)==t.foreground || distance(theme_pen(&t,i),t.background)>=40);
    puts("theme pens PASS: unchanged for the built-in theme, accent and dim pens guarded");
}

static void pen_keys(void)
{
    XTheme t,saved;
    theme_default(&t);
    assert(!theme_parse(&t,"background=#959595\nforeground=#000000\npen2=#FFFFFF\npen3=#3B67A2\n"
                           "pen4=#7B7B7B\npen5=#AFAFAF\npen6=#AA907C\npen7=#FFA997\n"));
    assert(theme_pen(&t,0)==0x959595 && theme_pen(&t,1)==0x000000);
    assert(theme_pen(&t,2)==0xffffff && theme_pen(&t,3)==0x3b67a2 && theme_pen(&t,4)==0x7b7b7b);
    assert(theme_pen(&t,5)==0xafafaf && theme_pen(&t,6)==0xaa907c && theme_pen(&t,7)==0xffa997);
    /* A given pen is used as it is, without guards. */
    theme_default(&t);
    assert(!theme_parse(&t,"pen4=#101820\n"));
    assert(theme_pen(&t,4)==0x101820);
    saved=t;
    assert(theme_parse(&t,"pen1=#ffffff\n"));
    assert(theme_parse(&t,"pen0=#ffffff\n"));
    assert(theme_parse(&t,"pen8=#ffffff\n"));
    assert(theme_parse(&t,"pen3=#ffffff\npen3=#000000\n"));
    assert(theme_parse(&t,"pen=#ffffff\n"));
    assert(theme_parse(&t,"colour=#ffffff\n"));
    assert(theme_parse(&t,"pen22=#ffffff\n"));
    assert(!memcmp(&t,&saved,sizeof t));
    puts("theme pen keys PASS: pen2..pen7, used unguarded, bad keys refused");
}

static void bold_switch(void)
{
    static XTermScreen s;
    static XTermCell cells[80];
    XTheme t;
    const char *seq="\033[1;31mB\033[22;31mN\033[1;38;5;2mP\033[1;91mH";
    theme_default(&t);
    assert(t.bold_bright==1);
    assert(!screen_init(&s,cells,80,2,40));
    screen_write(&s,(const unsigned char *)seq,strlen(seq));
    assert(theme_fg(&t,&cells[0])==t.ansi[9]);
    assert(theme_fg(&t,&cells[1])==t.ansi[1]);
    assert(theme_fg(&t,&cells[2])==t.ansi[10]);
    t.bold_bright=0;
    assert(theme_fg(&t,&cells[0])==t.ansi[1]);
    assert(theme_fg(&t,&cells[2])==t.ansi[2]);
    assert(theme_fg(&t,&cells[3])==t.ansi[9]);
    puts("theme bold PASS: bright on by default, off keeps the colour asked for");
}

int main(void)
{
    assert(theme_luma(0x000000)==0 && theme_luma(0xffffff)==255);
    assert(theme_luma(0xff0000)==76 && theme_luma(0x00ff00)==149 && theme_luma(0x0000ff)==29);
    roles_derived();
    pens_guarded();
    pen_keys();
    bold_switch();
    return 0;
}
