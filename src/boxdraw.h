#ifndef XTERM_BOXDRAW_H
#define XTERM_BOXDRAW_H
#include <stdint.h>
/* -1: use the font; 0/1: background/foreground in a supported box cell. */
static int boxdraw_pixel(uint32_t ch,unsigned w,unsigned h,unsigned x,unsigned y)
{
    unsigned arms;
    if(!w || !h || x>=w || y>=h) return -1;
    /* bits: left, right, up, down */
    switch(ch) {
    case 0x2500: arms=3; break;
    case 0x2502: arms=12; break;
    case 0x250c: arms=10; break;
    case 0x2510: arms=9; break;
    case 0x2514: arms=6; break;
    case 0x2518: arms=5; break;
    case 0x251c: arms=14; break;
    case 0x2524: arms=13; break;
    case 0x252c: arms=11; break;
    case 0x2534: arms=7; break;
    case 0x253c: arms=15; break;
    default: return -1;
    }
    return (y==h/2 && (((arms&1) && x<=w/2) || ((arms&2) && x>=w/2))) ||
           (x==w/2 && (((arms&4) && y<=h/2) || ((arms&8) && y>=h/2)));
}
#endif
