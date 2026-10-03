#include "../src/boxdraw.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    unsigned x,y,w,h;
    for(w=7;w<=16;++w) for(h=13;h<=32;++h) {
        for(y=0;y<h;++y) assert(boxdraw_pixel(0x2502,w,h,w/2,y)==1);
        for(x=0;x<w;++x) assert(boxdraw_pixel(0x2500,w,h,x,h/2)==1);
        assert(boxdraw_pixel(0x250c,w,h,w-1,h/2)==1);
        assert(boxdraw_pixel(0x250c,w,h,w/2,h-1)==1);
        assert(boxdraw_pixel(0x250c,w,h,0,h/2)==0);
        assert(boxdraw_pixel(0x250c,w,h,w/2,0)==0);
        assert(boxdraw_pixel(0x253c,w,h,w/2,0)==1);
        assert(boxdraw_pixel(0x253c,w,h,0,h/2)==1);
    }
    assert(boxdraw_pixel('A',8,17,0,0)==-1);
    assert(boxdraw_pixel(0x2500,0,17,0,0)==-1);
    puts("boxdraw PASS: continuous cell edges, corners, junctions, fallback");
}
