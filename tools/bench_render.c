/* CPU microbenchmark, not an Intuition scrolling/FPS measurement.
 * Same fonts, glyph cache and RGB compositor as the native preview. */
#include "../src/aros_font.h"
#include "../src/cluster_font.h"
#include <proto/dos.h>
#include <stdio.h>

static unsigned long ticks(void)
{
    struct DateStamp t;
    DateStamp(&t);
    return ((unsigned long)t.ds_Days*24*60+t.ds_Minute)*60*TICKS_PER_SECOND+t.ds_Tick;
}
int main(void)
{
    static const char *const paths[5]={"FONTS:TrueType/VeraMono.ttf",
        "FONTS:TrueType/OpenSans-Regular.ttf","PROGDIR:Fonts/NotoEmoji.ttf",
        "PROGDIR:Fonts/Unifont.otf","PROGDIR:Fonts/NotoColorEmoji.ttf"};
    static const uint32_t text[]={ 'A','R','O','S',' ','t','e','r','m','i','n','a','l',
        0x4e2d,0x6587,0x1f600,0x0105,0x20ac };
    unsigned char rgb[XRASTER_MAX*XRASTER_MAX*3];
    unsigned i,j; unsigned long start,elapsed; uint32_t digest=2166136261u;
    if(xfont_open_extended(paths)) return 20;
    start=ticks();
    for(i=0;i<2000;++i) for(j=0;j<sizeof text/sizeof text[0];++j) {
        unsigned span=(text[j]==0x4e2d || text[j]==0x6587 || text[j]==0x1f600)?2:1;
        const XClusterRaster *r=xfont_cluster(&text[j],1,span);
        unsigned k;
        xfont_cluster_rgb(r,0xabb2bf,0x101820,rgb);
        /* Sparse checksum keeps rendered bytes observable without timing a
         * full-buffer hash instead of the compositor. */
        for(k=0;k<r->width*r->height*3;k+=31) digest=(digest^rgb[k])*16777619u;
    }
    elapsed=ticks()-start;
    printf("render CPU: 36000 cells, %lu ticks at %u Hz, checksum %08lx\n",
        elapsed,(unsigned)TICKS_PER_SECOND,(unsigned long)digest);
    xfont_close(); return 0;
}
