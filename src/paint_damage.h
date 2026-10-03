#ifndef XTERM_PAINT_DAMAGE_H
#define XTERM_PAINT_DAMAGE_H
#include "screen_model.h"
#define XPAINT_MAX_ROWS 80
enum { XPAINT_CURSOR=1, XPAINT_UNDERLINE=2 };
typedef struct { XTermCell cell; uint32_t fg,bg,cursor_colour; unsigned flags; } XPaintCell;
typedef struct {
    XPaintCell *cells;
    size_t capacity;
    unsigned rows,columns;
    unsigned char valid[XPAINT_MAX_ROWS];
} XPaintDamage;
void paint_damage_init(XPaintDamage *,XPaintCell *,size_t);
int paint_damage_geometry(XPaintDamage *,unsigned,unsigned);
void paint_damage_invalidate(XPaintDamage *);
void paint_damage_forget_row(XPaintDamage *,unsigned);
int paint_damage_changed(const XPaintDamage *,unsigned,const XPaintCell *);
int paint_damage_commit(XPaintDamage *,unsigned,const XPaintCell *);
#endif
