#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[2400];
static char reply[128];
static size_t length;
static void collect(void *context,const unsigned char *p,size_t n)
{
    assert(context==&length && length+n<sizeof reply);
    memcpy(reply+length,p,n); length+=n; reply[length]=0;
}
static void text(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
int main(void)
{
    const char query[]="\033[18t\033[0 q";
    size_t split;
    for(split=0;split<sizeof query;++split) {
        assert(!screen_init(&s,cells,2400,24,80));
        s.reply=collect; s.reply_context=&length; length=0;
        text("abc");
        screen_write(&s,(const unsigned char *)query,split);
        screen_write(&s,(const unsigned char *)query+split,sizeof query-1-split);
        /* Native WSR conflicts with VT DECSCUSR: never answer cursor style. */
        assert(!strcmp(reply,"\033[8;24;80t"));
        assert(s.cursor_row==0 && s.cursor_column==3 && screen_cell(&s,0,2)=='c');
        assert(!screen_resize(&s,20,100)); length=0;
        text("\033[18t"); assert(!strcmp(reply,"\033[8;20;100t"));
        length=0;
        text("\033[?18t\033[18;1t\033[18 t\033[0  q\033[ 18t\033[1 q");
        assert(length==0);
        s.reply=NULL; text("\033[18t"); assert(length==0);
    }
    puts("geometry reply PASS: split queries, VT size, resize, no cursor-style replies, strict parsing, opt-in");
    return 0;
}
