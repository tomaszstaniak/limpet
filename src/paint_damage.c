#include "paint_damage.h"
#include <string.h>
void paint_damage_init(XPaintDamage *d,XPaintCell *cells,size_t capacity)
{ memset(d,0,sizeof *d); d->cells=cells; d->capacity=capacity; }
void paint_damage_invalidate(XPaintDamage *d) { memset(d->valid,0,sizeof d->valid); }
void paint_damage_forget_row(XPaintDamage *d,unsigned r)
{ if(r<d->rows) d->valid[r]=0; }
int paint_damage_geometry(XPaintDamage *d,unsigned rows,unsigned columns)
{
    if(!d->cells || !rows || rows>XPAINT_MAX_ROWS || !columns || rows>d->capacity/columns) {
        paint_damage_invalidate(d); d->rows=d->columns=0; return 0;
    }
    if(d->rows!=rows || d->columns!=columns) paint_damage_invalidate(d);
    d->rows=rows; d->columns=columns; return 1;
}
static int equal(const XPaintCell *a,const XPaintCell *b)
{
    unsigned i;
    if(a->fg!=b->fg || a->bg!=b->bg || a->flags!=b->flags ||
       ((a->flags&XPAINT_CURSOR) && a->cursor_colour!=b->cursor_colour) ||
       a->cell.ch!=b->cell.ch || a->cell.width!=b->cell.width ||
       a->cell.length!=b->cell.length) return 0;
    if(a->cell.length>16) return 0;
    for(i=1;i<a->cell.length;++i)
        if(a->cell.suffix[i-1]!=b->cell.suffix[i-1]) return 0;
    return 1;
}
int paint_damage_changed(const XPaintDamage *d,unsigned r,const XPaintCell *row)
{
    unsigned c;
    if(!row || !d->columns || r>=d->rows || !d->valid[r]) return 1;
    for(c=0;c<d->columns;++c)
        if(!equal(d->cells+(size_t)r*d->columns+c,row+c)) return 1;
    return 0;
}
int paint_damage_commit(XPaintDamage *d,unsigned r,const XPaintCell *row)
{
    if(!row || !d->columns || r>=d->rows) return 0;
    memcpy(d->cells+(size_t)r*d->columns,row,d->columns*sizeof *row);
    d->valid[r]=1; return 1;
}
