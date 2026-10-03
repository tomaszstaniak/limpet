#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XTermScreen s;
static XTermCell cells[120], alternate[120], history[120];
static char response[256];
static size_t used;
static void collect(void *ctx,const unsigned char *p,size_t n)
{
    assert(ctx==&used && used+n<sizeof response);
    memcpy(response+used,p,n); used+=n; response[used]=0;
}
static void text(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
static void init(unsigned rows,unsigned columns)
{
    assert(!screen_init(&s,cells,120,rows,columns));
    s.reply=collect; s.reply_context=&used; used=0; response[0]=0;
}

/* Missing reply, zero-based coordinates, or query-triggered wrap breaks this. */
static void reports(void)
{
    const char *query="\033[5n\033[6n\033[?6n";
    size_t split;
    for(split=0;split<=strlen(query);++split) {
        init(4,10); text("\033[2;10HX");
        assert(s.wrap_pending);
        screen_write(&s,(const unsigned char *)query,split);
        screen_write(&s,(const unsigned char *)query+split,strlen(query)-split);
        assert(!strcmp(response,"\033[0n\033[2;10R\033[?2;10R"));
        assert(s.wrap_pending && s.cursor_row==1 && s.cursor_column==9);
        text("Y"); assert(screen_cell(&s,2,0)=='Y');
    }
    init(4,10); text("\033[4;10H");
    assert(!screen_resize(&s,2,6)); text("\033[6n");
    assert(!strcmp(response,"\033[2;6R"));
    assert(!screen_attach_alternate(&s,alternate,120));
    used=0; text("\033[?1049h\033[6n\033[?1049l\033[6n");
    assert(!strcmp(response,"\033[1;1R\033[2;6R"));
    init(4,10); assert(!screen_attach_history(&s,history,120));
    text("a\r\nb\r\nc\r\nd\r\ne"); screen_scroll_view(&s,1);
    text("\033[6n"); assert(!strcmp(response,"\033[4;2R"));
    /* A cursor on a wide continuation still reports a physical column. */
    init(4,10); text("\xe4\xb8\xad\033[1;2H\033[6n");
    assert(!strcmp(response,"\033[1;2R"));
}

/* Unsupported/malformed requests must not masquerade as status inquiries. */
static void strict_queries(void)
{
    init(4,10);
    text("\033[n\033[0n\033[7n\033[?5n\033[>6n\033[6;n\033[6;1n"
         "\033[6 n\033[ 6n\033[6:1n\033[999999999999n"
         "\033[6;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0n"
         "\033[6\030n\033[6\032n\033P\033[6n\033\\");
    assert(!used);
    s.reply=NULL; text("\033[5n\033[6n\033[?6n"); assert(!used);
}

/* Wrong axis, zero defaults, failure to clamp, or scrolling breaks literals. */
static void motions(void)
{
    static const struct {const char *sequence; unsigned row,column;} cases[]={
        {"\033[G",2,0},{"\033[0G",2,0},{"\033[7G",2,6},
        {"\033[999999999999G",2,9},{"\033[3`",2,2},
        {"\033[d",0,4},{"\033[0d",0,4},{"\033[4d",3,4},
        {"\033[999999999999d",3,4},
        {"\033[E",3,0},{"\033[0E",3,0},{"\033[999999999999E",3,0},
        {"\033[F",1,0},{"\033[0F",1,0},{"\033[2F",0,0},
        {"\033[999999999999F",0,0}
    };
    size_t i,split;
    for(i=0;i<sizeof cases/sizeof cases[0];++i) {
        for(split=0;split<=strlen(cases[i].sequence);++split) {
            XTermCell before[40];
            init(4,10); text("0123456789\r\nabcdefghij\033[3;5H");
            memcpy(before,cells,sizeof before);
            screen_write(&s,(const unsigned char *)cases[i].sequence,split);
            screen_write(&s,(const unsigned char *)cases[i].sequence+split,
                         strlen(cases[i].sequence)-split);
            assert(s.cursor_row==cases[i].row && s.cursor_column==cases[i].column);
            assert(!memcmp(before,cells,sizeof before));
            assert(!s.wrap_pending);
        }
    }
    init(4,10); text("0123456789\033[GX");
    assert(screen_cell(&s,0,0)=='X' && screen_cell(&s,1,0)==' ');
    init(1,1); text("X\033[999E\033[999F\033[999G\033[999d\033[6n");
    assert(!strcmp(response,"\033[1;1R") && screen_cell(&s,0,0)=='X');
}
int main(void)
{
    reports(); strict_queries(); motions();
    puts("cursor/query PASS: split input, strict replies, live coordinates, wrap, resize, alternate, clamped motion");
    return 0;
}
