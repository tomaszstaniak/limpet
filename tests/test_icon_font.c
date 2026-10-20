/* Bounded actual-font check; direct stdout to a retained result file. */
#include "../src/eza_icon_contract.h"
#include "../src/screen_model.h"
#include <stdio.h>
int main(int argc,char **argv)
{
    const char *paths[4];unsigned i,clipped=0,features;
    if(argc!=5) { puts("Usage: test_icon_font primary secondary emoji unifont");return 20; }
    for(i=0;i<4;++i) paths[i]=argv[i+1];
    if(xfont_open_paths(paths)) return 20;
    for(i=0;i<sizeof eza_icon_scalars/sizeof *eza_icon_scalars;++i) {
        const XFontGlyph *g=xfont_glyph(eza_icon_scalars[i]);
        if(g->left<0 || g->top<0 || (unsigned)g->left+g->width>xfont_width() ||
           (unsigned)g->top+g->height>xfont_height()) ++clipped;
    }
    features=eza_icon_features(xfont_glyph,screen_scalar_width,xfont_width());
    printf("eza=0.23.5 icons=%u features=%u grid=%ux%u clipped=%u\n",
           (unsigned)(sizeof eza_icon_scalars/sizeof *eza_icon_scalars),features,
           xfont_width(),xfont_height(),clipped);
    xfont_close();return features==12 && !clipped ? 0:10;
}
