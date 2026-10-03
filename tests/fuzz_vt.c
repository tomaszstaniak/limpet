/* Host-only bounded stateful fuzz target. No GUI, DOS or Ghostty dependency.
 * Compare bulk and bytewise delivery at identical external event boundaries;
 * also assert structural invariants, independent of that comparison. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define CELL_COUNT 256
#define INPUT_LIMIT 4096
typedef struct {
    XTermScreen s;
    XTermCell cells[CELL_COUNT], alternate[CELL_COUNT], history[CELL_COUNT];
    XHyperlinks links;
    unsigned char replies[INPUT_LIMIT*XSCREEN_REPLY_CAPACITY];
    size_t used;
} Machine;
static Machine machines[2];
static void collect(void *context,const unsigned char *bytes,size_t n)
{
    Machine *m=context;
    assert(n<=XSCREEN_REPLY_CAPACITY && n<=sizeof m->replies-m->used);
    memcpy(m->replies+m->used,bytes,n); m->used+=n;
}
static void init(Machine *m)
{
    memset(m,0,sizeof *m);
    assert(!screen_init(&m->s,m->cells,CELL_COUNT,4,16));
    assert(!screen_attach_history(&m->s,m->history,CELL_COUNT));
    assert(!screen_attach_alternate(&m->s,m->alternate,CELL_COUNT));
    hyperlinks_init(&m->links); screen_attach_links(&m->s,&m->links);
    m->s.reply=collect; m->s.reply_context=m;
}
static void same_cell(XTermCell a,XTermCell b)
{
    assert(a.ch==b.ch && a.width==b.width && a.length==b.length);
    assert(a.fg==b.fg && a.bg==b.bg && a.bold==b.bold);
    assert(a.fg_rgb==b.fg_rgb && a.bg_rgb==b.bg_rgb);
    assert(a.wrap==b.wrap && a.colour_flags==b.colour_flags && a.link_id==b.link_id);
    assert(!memcmp(a.suffix,b.suffix,sizeof a.suffix));
}
static void check(void)
{
    XTermScreen *a=&machines[0].s,*b=&machines[1].s;
    unsigned r,c;
    assert(a->rows==b->rows && a->columns==b->columns);
    assert(a->cursor_row<a->rows && a->cursor_column<a->columns);
    assert(a->cursor_row==b->cursor_row && a->cursor_column==b->cursor_column);
    assert(a->history_count<=a->history_lines && a->view_offset<=a->history_count);
    assert(a->history_count==b->history_count && a->view_offset==b->view_offset);
    assert(a->alternate_active==b->alternate_active && a->wrap_pending==b->wrap_pending);
    assert(a->active_link==b->active_link && a->cursor_visible==b->cursor_visible);
    assert(a->mouse_tracking==b->mouse_tracking && a->mouse_sgr==b->mouse_sgr);
    assert(a->osc_length<XHYPERLINK_LIMIT && a->param_count<=16);
    same_cell(a->style,b->style);
    assert(machines[0].used==machines[1].used);
    assert(!memcmp(machines[0].replies,machines[1].replies,machines[0].used));
    for(r=0;r<a->history_count+a->rows;++r) for(c=0;c<a->columns;++c) {
        XTermCell cell=screen_line_cell(a,r,c);
        same_cell(cell,screen_line_cell(b,r,c));
        assert(cell.width<=2 && cell.length<=16);
        if(!cell.length) assert(cell.ch==' ');
        if(cell.width==0) {
            assert(c>0 && cell.ch==' ' && !cell.length);
            assert(screen_line_cell(a,r,c-1).width==2);
        } else if(cell.width==2) {
            assert(c+1<a->columns && screen_line_cell(a,r,c+1).width==0);
        }
    }
    for(r=0;r<a->rows;++r) for(c=0;c<a->columns;++c) {
        XTermCell expected=screen_line_cell(a,a->history_count-a->view_offset+r,c);
        same_cell(screen_view_cell(a,r,c),expected);
        same_cell(screen_view_cell(b,r,c),expected);
    }
}
static void feed(const unsigned char *p,size_t n)
{
    size_t i;
    screen_write(&machines[0].s,p,n);
    for(i=0;i<n;++i) screen_write(&machines[1].s,p+i,1);
    check();
}
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size)
{
    size_t pos=1,n;
    unsigned op,i;
    if(!size || size>INPUT_LIMIT) return 0;
    init(&machines[0]); init(&machines[1]);
    if(!(data[0]&1)) { feed(data+1,size-1); return 0; }
    while(pos<size) {
        op=data[pos++]%8;
        if(op<4) {
            if(pos==size) break;
            n=1+data[pos++]%64; if(n>size-pos) n=size-pos;
            feed(data+pos,n); pos+=n;
        } else if(op==4) {
            unsigned rows,cols;
            if(size-pos<2) break;
            rows=1+data[pos++]%8; cols=1+data[pos++]%32;
            for(i=0;i<2;++i) assert(!screen_resize(&machines[i].s,rows,cols));
        } else if(op==5) {
            int delta;
            if(pos==size) break;
            delta=(int)data[pos++]-128;
            for(i=0;i<2;++i) screen_scroll_view(&machines[i].s,delta);
        } else {
            const char *sequence=op==6?"\033[?1049h":"\033[?1049l";
            feed((const unsigned char *)sequence,strlen(sequence));
        }
        check();
    }
    return 0;
}
#ifdef VT_FUZZ_SMOKE
int main(void)
{
    static const unsigned char stream[]={0,27,'[','3',0,'1','m','X',27,'[','6','n'};
    static const unsigned char events[]={1,0,3,0xe7,0x95,0x8c,'X',6,4,0,0,7,5,255};
    LLVMFuzzerTestOneInput(stream,sizeof stream);
    assert(screen_cell(&machines[0].s,0,0)=='X' && machines[0].s.style.fg==1);
    assert(machines[0].used==6 && !memcmp(machines[0].replies,"\033[1;2R",6));
    LLVMFuzzerTestOneInput(events,sizeof events);
    LLVMFuzzerTestOneInput(NULL,0);
    puts("VT fuzz harness smoke PASS");
    return 0;
}
#endif
