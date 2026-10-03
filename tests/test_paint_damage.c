#include "../src/paint_damage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XPaintCell storage[12], row[4];
static XPaintDamage damage;
int main(void)
{
    memset(row,0,sizeof row);
    row[0].cell.ch='A'; row[0].cell.width=1; row[0].cell.length=1;
    row[0].fg=0xffffff; row[0].bg=0x101820;
    paint_damage_init(&damage,storage,12);
    assert(paint_damage_geometry(&damage,3,4));
    assert(paint_damage_changed(&damage,0,row));
    assert(paint_damage_commit(&damage,0,row));
    assert(!paint_damage_changed(&damage,0,row));
    paint_damage_forget_row(&damage,0); /* Partial upload of B, then model returns to A. */
    assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    assert(paint_damage_changed(&damage,1,row));
    row[0].cell.ch='B'; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].cell.length=2; row[0].cell.suffix[0]=0x301;
    assert(paint_damage_changed(&damage,0,row)); paint_damage_commit(&damage,0,row);
    row[0].cell.suffix[0]=0x308;
    assert(paint_damage_changed(&damage,0,row)); paint_damage_commit(&damage,0,row);
    row[0].fg=0; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].bg=0xabcdef; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].flags=XPAINT_UNDERLINE; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].flags|=XPAINT_CURSOR; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].flags=0; assert(paint_damage_changed(&damage,0,row));
    paint_damage_commit(&damage,0,row);
    row[0].cell.link_id=42; row[0].cell.wrap=1;
    assert(!paint_damage_changed(&damage,0,row)); /* Nonvisual metadata. */
    paint_damage_invalidate(&damage);
    assert(paint_damage_changed(&damage,0,row)); paint_damage_commit(&damage,0,row);
    assert(paint_damage_geometry(&damage,3,4));
    assert(!paint_damage_changed(&damage,0,row));
    assert(paint_damage_geometry(&damage,2,4));
    assert(paint_damage_changed(&damage,0,row));
    assert(!paint_damage_geometry(&damage,0,4));
    assert(!paint_damage_geometry(&damage,4,4));
    assert(!paint_damage_commit(&damage,99,row));
    assert(paint_damage_changed(&damage,99,row));
    puts("paint damage: PASS");
    return 0;
}
