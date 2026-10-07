#include "selection.h"
#include "utf8proc.h"
#include <string.h>
int selection_hit(const XTermScreen *s,const XSelectionGrid *g,int x,int y,int clamp,unsigned *row,unsigned *col)
{
    int width,height;
    if(!s || !g || !g->width || !g->height || !s->columns || !s->rows || !row || !col) return 0;
    x-=g->left; y-=g->top;
    width=(int)(s->columns*g->width); height=(int)(s->rows*g->height);
    if(!clamp && (x<0 || y<0 || x>=width || y>=height)) return 0;
    if(x<0) x=0;
    if(y<0) y=0;
    if(x>=width) x=width-1;
    if(y>=height) y=height-1;
    *col=(unsigned)x/g->width; *row=(unsigned)y/g->height;
    return 1;
}

static unsigned lead(const XTermScreen *s,unsigned row,unsigned col)
{
    if (!screen_line_cell(s,row,col).width && col) --col;
    return col;
}
void selection_start(XSelection *a,const XTermScreen *s,unsigned row,unsigned col)
{
    if (!a || !s || row>=s->rows || col>=s->columns) return;
    a->anchor_row=a->end_row=s->history_count-s->view_offset+row;
    a->anchor_col=a->end_col=lead(s,a->anchor_row,col);
    a->active=a->dragging=1;
}
void selection_extend(XSelection *a,const XTermScreen *s,unsigned row,unsigned col)
{
    if (!a || !a->active || !s || row>=s->rows || col>=s->columns) return;
    a->end_row=s->history_count-s->view_offset+row;
    a->end_col=lead(s,a->end_row,col);
}
static int bounds(const XSelection *a,const XTermScreen *s,size_t *start,size_t *end)
{
    size_t other;
    if (!a || !s || !a->active || a->anchor_row>=s->history_count+s->rows ||
        a->end_row>=s->history_count+s->rows || a->anchor_col>=s->columns ||
        a->end_col>=s->columns) return -1;
    *start=(size_t)a->anchor_row*s->columns+lead(s,a->anchor_row,a->anchor_col);
    *end=(size_t)a->end_row*s->columns+lead(s,a->end_row,a->end_col);
    if (*start>*end) { other=*start; *start=*end; *end=other; }
    *end+=screen_line_cell(s,(unsigned)(*end/s->columns),(unsigned)(*end%s->columns)).width;
    return 0;
}
int selection_contains(const XSelection *a,const XTermScreen *s,unsigned row,unsigned col)
{
    size_t first,end,index;
    if (!s || row>=s->rows || col>=s->columns || bounds(a,s,&first,&end)) return 0;
    index=(size_t)(s->history_count-s->view_offset+row)*s->columns+col;
    return index>=first && index<end;
}
int selection_text(const XSelection *a,const XTermScreen *s,unsigned char *out,size_t cap,size_t *written)
{
    size_t first,end,total=0; unsigned pass,row;
    if (!written) return -1;
    *written=0;
    if ((!out && cap) || bounds(a,s,&first,&end)) return -1;
    /* Two passes guarantee no partial output on capacity failure. */
    for (pass=0;pass<2;++pass) {
        total=0;
        for (row=(unsigned)(first/s->columns);row<=(end-1)/s->columns;++row) {
            unsigned col,lo=row==first/s->columns?(unsigned)(first%s->columns):0;
            unsigned hi=row==(end-1)/s->columns?(unsigned)((end-1)%s->columns)+1:s->columns;
            XTermCell tail=screen_line_cell(s,row,s->columns-1);
            /* Trim only unwritten cells: explicitly typed spaces are data. */
            while (hi>lo) {
                XTermCell c=screen_line_cell(s,row,hi-1);
                if (!c.width || c.length) break;
                --hi;
            }
            for (col=lo;col<hi;++col) {
                XTermCell c=screen_line_cell(s,row,col);
                unsigned i,count=c.length?c.length:1;
                if (!c.width) continue;
                for (i=0;i<count;++i) {
                    unsigned char bytes[4];
                    utf8proc_ssize_t n=utf8proc_encode_char((int32_t)(i?c.suffix[i-1]:c.ch),bytes);
                    if (n<=0) return -1;
                    if ((size_t)n>cap-total) return -2;
                    if (pass) memcpy(out+total,bytes,(size_t)n);
                    total+=(size_t)n;
                }
            }
            if (row<(end-1)/s->columns && !tail.wrap) {
                if (total==cap) return -2;
                if (pass) out[total]='\n';
                ++total;
            }
        }
    }
    *written=total; return 0;
}

int selection_keep(const XSelection *sel, const XTermScreen *s, const unsigned char *before,
                   size_t before_length, unsigned long scroll_before)
{
    unsigned char now[1024];
    size_t n=0;
    if(!sel || !s || !sel->active || s->scroll_count!=scroll_before) return 0;
    if(selection_text(sel,s,now,sizeof now,&n)) return 0;
    return n==before_length && !memcmp(now,before,n);
}
