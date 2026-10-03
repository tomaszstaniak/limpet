/* Exercise the actual private bitmap reader; no production test API. Link
 * with font_cache.c and HarfBuzz, not a second copy of cluster_font.c. */
#include "../src/cluster_font.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned char gray[]={10,99,200,99};
    unsigned char mono[]={0x40,0x80};
    unsigned char bgra[]={0,0,128,128, 32,16,0,64};
    FT_Bitmap b;
    memset(&b,0,sizeof b);
    b.width=1; b.rows=2; b.pitch=-2;
    b.buffer=gray; b.pixel_mode=FT_PIXEL_MODE_GRAY; b.num_grays=256;
    /* Buffer begins with the bottom row for negative pitch. Padding is not
     * a pixel; top and bottom must both remain inside the allocation. */
    assert(bitmap_pixel(&b,0,0)==0xc8c8c8c8);
    assert(bitmap_pixel(&b,0,1)==0x0a0a0a0a);
    b.pitch=2;
    assert(bitmap_pixel(&b,0,0)==0x0a0a0a0a);
    assert(bitmap_pixel(&b,0,1)==0xc8c8c8c8);
    b.width=2; b.pitch=-1; b.buffer=mono; b.pixel_mode=FT_PIXEL_MODE_MONO;
    assert(bitmap_pixel(&b,0,0)==0xffffffff && bitmap_pixel(&b,1,0)==0);
    assert(bitmap_pixel(&b,0,1)==0 && bitmap_pixel(&b,1,1)==0xffffffff);
    b.width=1; b.pitch=-4; b.buffer=bgra; b.pixel_mode=FT_PIXEL_MODE_BGRA;
    assert(bitmap_pixel(&b,0,0)==0x40001020);
    assert(bitmap_pixel(&b,0,1)==0x80800000);
    puts("cluster bitmap tests passed (positive/negative pitch, padding, GRAY/MONO/BGRA)");
    return 0;
}
