/* Characterization of the existing application-owned status panel protocol.
 * Catches missing CUP/EL, stale wide glyphs, status-driven scrolling and lost
 * primary state. No reserved terminal chrome or scrolling margins implied. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen screen;
static XTermCell cells[100], alternate[100], history[100], primary[100];
static void text(const char *s)
{ screen_write(&screen,(const unsigned char *)s,strlen(s)); }
static void row(unsigned y,const char *prefix)
{
    unsigned x;
    size_t n=strlen(prefix);
    for(x=0;x<20;++x)
        assert(screen_cell(&screen,y,x)==(x<n?(unsigned char)prefix[x]:' '));
}
int main(void)
{
    const unsigned char update[]="\033[4;1Hdone\033[K\033[5;1H中😀\033[K\033[3;3H";
    size_t split;
    for(split=0;split<sizeof update;++split) {
        unsigned i;
        assert(!screen_init(&screen,cells,100,5,20));
        assert(!screen_attach_history(&screen,history,100));
        assert(!screen_attach_alternate(&screen,alternate,100));
        text("shell> "); memcpy(primary,cells,sizeof cells);
        text("\033[?1049h\033[1;1Hlog one\033[K\033[2;1Hlog two\033[K"
             "\033[3;1H> draft\033[K\033[4;1Hworking 100%\033[K"
             "\033[5;1Htool: pending\033[K");
        screen_write(&screen,update,split);
        screen_write(&screen,update+split,sizeof update-1-split);
        row(0,"log one"); row(1,"log two"); row(2,"> draft"); row(3,"done");
        assert(screen_cell(&screen,4,0)==0x4e2d && cells[4*20].width==2);
        assert(screen_cell(&screen,4,2)==0x1f600 && cells[4*20+2].width==2);
        for(i=4;i<20;++i) assert(screen_cell(&screen,4,i)==' ');
        assert(screen.cursor_row==2 && screen.cursor_column==2);
        /* Repeated shrinking updates, including replacement of wide glyphs. */
        for(i=0;i<100;++i) {
            text("\033[4;1Hworking 100%\033[K\rOK\033[K"
                 "\033[5;1Hx\033[K\033[3;3H");
            row(3,"OK"); row(4,"x");
            assert(!screen.history_count && !screen.wrap_pending);
        }
        row(0,"log one"); row(1,"log two"); row(2,"> draft");
        text("\033[?1049l");
        assert(!screen.alternate_active && !memcmp(primary,cells,sizeof cells));
        assert(screen.cursor_row==0 && screen.cursor_column==7);
    }
    puts("status area PASS: split UTF-8/CSI, in-place shrink, wide erase, stable body, primary restore");
    return 0;
}
