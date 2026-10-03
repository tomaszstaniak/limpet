#include "../src/aros_font.h"
#include "../src/cluster_font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(const uint32_t *s, unsigned n, unsigned span, int colour)
{
    const XClusterRaster *r = xfont_cluster(s,n,span);
    unsigned i, ink=0, chroma=0;
    printf("cluster U+%04X length=%u source=%u glyphs=%u status=%u colour=%u %ux%u\n",
           (unsigned)s[0],n,r->source,r->glyphs,r->status,r->colour,r->width,r->height);
    fflush(stdout);
    assert(r->status==XFONT_OK && r->glyphs==1);
    assert(r->width==8*span && r->height==17);
    assert(r->colour==(unsigned)colour);
    for(i=0;i<r->width*r->height;++i) {
        uint32_t p=r->pixels[i];
        ink |= p>>24;
        chroma |= ((p>>16)&255)!=((p>>8)&255) || ((p>>8)&255)!=(p&255);
    }
    printf("ink=%u chroma=%u\n",ink,chroma); fflush(stdout);
    /* Modern Noto family artwork is intentionally neutral/grey. Require
     * chromatic pixels for the independently recognizable smile sample. */
    assert(ink && (s[0]!=0x1f600 || !colour || chroma));
}
static void check_ignorables(void)
{
    const uint32_t scalars[]={0x200b,0x200c,0x200d,0xfe0f,0xad,0x34f,0x202e};
    unsigned i,j;
    /* A successful zero-glyph shape must stay invisible, not draw U+FFFD. */
    for(i=0;i<sizeof scalars/sizeof scalars[0];++i) {
        const XClusterRaster *r=xfont_cluster(&scalars[i],1,1);
        unsigned char rgb[XRASTER_MAX*XRASTER_MAX*3];
        assert(r->status==XFONT_OK && r->glyphs==0);
        assert(r->width==8 && r->height==17);
        xfont_cluster_rgb(r,0xffffff,0x123456,rgb);
        for(j=0;j<r->width*r->height;++j) {
            assert(r->pixels[j]==0);
            assert(rgb[3*j]==0x12 && rgb[3*j+1]==0x34 && rgb[3*j+2]==0x56);
        }
    }
}
int main(int argc,char **argv)
{
    const char *paths[5]; unsigned i;
    const uint32_t accent[]={ 'e',0x301 };
    const uint32_t family[]={0x1f468,0x200d,0x1f469,0x200d,0x1f467,0x200d,0x1f466};
    const uint32_t flag[]={0x1f1f5,0x1f1f1};
    const uint32_t skin[]={0x1f44d,0x1f3fd};
    const uint32_t heart[]={0x2764,0xfe0f};
    const uint32_t keycap[]={'1',0xfe0f,0x20e3};
    const uint32_t smile[]={0x1f600};
    unsigned char rgb[32*32*3]; XClusterRaster saved;
    assert(argc==6);
    for(i=0;i<5;++i) paths[i]=argv[i+1];
    assert(xfont_open_extended(paths)==0);
    check_ignorables();
    check(accent,2,1,0); check(family,7,2,1); check(flag,2,2,1);
    check(skin,2,2,1); check(heart,2,2,1); check(keycap,3,2,1); check(smile,1,2,1);
    saved=*xfont_cluster(family,7,2);
    for(i=32;i<400;++i) { uint32_t s=i; (void)xfont_cluster(&s,1,1); }
    assert(!memcmp(saved.pixels,xfont_cluster(family,7,2)->pixels,sizeof saved.pixels));
    memset(&saved,0,sizeof saved); saved.width=saved.height=1;
    saved.colour=1; saved.pixels[0]=0x80800000;
    xfont_cluster_rgb(&saved,0xffffff,0x0000ff,rgb);
    assert(rgb[0]==128 && rgb[1]==0 && rgb[2]==127);
    xfont_close(); paths[4]=NULL;
    assert(xfont_open_extended(paths)==0);
    check(family,7,2,0); check(flag,2,2,0); check(keycap,3,2,0);
    assert(xfont_cluster(NULL,0,1)->status!=XFONT_OK);
    assert(xfont_cluster(smile,1,3)->status!=XFONT_OK);
    xfont_close();
    assert(xfont_cluster(smile,1,2)->status==XFONT_CLOSED);
    puts("cluster font tests passed (shaping, colour, fallback, bounds, cache, blend)");
    return 0;
}
