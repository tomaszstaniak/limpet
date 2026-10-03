#include "line_view.h"
#include "utf8proc.h"
#include <string.h>
int line_view_build(XTermScreen *view,const XTermScreen *base,const XLineEditor *e,
                    XTermCell *cells,size_t cap,XTermCell *history,size_t hcap)
{
    size_t points[XLINE_CAPACITY+1], count=1, pos=0, i;
    utf8proc_int32_t previous=0, state=0;
    unsigned cursor_row=0, cursor_col=0;
    size_t lo=e->anchor<e->cursor?e->anchor:e->cursor;
    size_t hi=e->anchor>e->cursor?e->anchor:e->cursor;
    if(!view || !base || !cells || !history || cap<base->capacity ||
       hcap<2*XLINE_CAPACITY || base->parser || base->utf8_remaining) return 0;
    points[0]=0;
    while(pos<e->length) {
        utf8proc_int32_t scalar;
        utf8proc_ssize_t used=utf8proc_iterate((const unsigned char *)e->text+pos,
                                              (utf8proc_ssize_t)(e->length-pos),&scalar);
        if(used<=0) return 0;
        if(pos && utf8proc_grapheme_break_stateful(previous,scalar,&state)) points[count++]=pos;
        previous=scalar; pos+=(size_t)used;
    }
    if(e->length) points[count++]=e->length;
    *view=*base;
    memcpy(cells,base->cells,base->capacity*sizeof *cells);
    view->cells=cells; view->capacity=cap;
    view->history=NULL; view->history_capacity=0;
    view->history_lines=view->history_count=view->history_head=view->view_offset=0;
    view->alternate=NULL; view->alternate_active=0; view->reflow_spare=NULL;
    if(screen_attach_history(view,history,hcap)) return 0;
    for(i=0;i+1<count;++i) {
        view->style=base->style;
        if(e->selecting && points[i]>=lo && points[i]<hi) {
            view->style.fg=15; view->style.bg=4; view->style.bold=0;
            view->style.colour_flags=XCOLOUR_SELECTED;
        }
        /* Input is a separate text run; it cannot combine with the prompt. */
        view->cluster_active=0; view->cluster_state=0;
        screen_write(view,(const unsigned char *)e->text+points[i],points[i+1]-points[i]);
        if(points[i]==e->cursor) {
            cursor_row=view->history_count+(unsigned)(view->cluster_cell/view->columns);
            cursor_col=(unsigned)(view->cluster_cell%view->columns);
        }
    }
    if(e->cursor==e->length) {
        cursor_row=view->history_count+view->cursor_row;
        cursor_col=view->cursor_column;
    }
    if(cursor_row<view->history_count) view->view_offset=view->history_count-cursor_row;
    view->cursor_row=cursor_row+view->view_offset-view->history_count;
    view->cursor_column=cursor_col;
    view->cursor_visible=1;
    return 1;
}
