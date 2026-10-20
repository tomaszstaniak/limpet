#include "../src/eza_icon_contract.h"
#include <assert.h>
static XFontGlyph g;
static const XFontGlyph *glyph(uint32_t scalar) { (void)scalar; return &g; }
static unsigned width(uint32_t scalar) { (void)scalar; return 1; }
int main(void) {
    assert(!eza_icon_features(glyph,width,8));
    g.status=XFONT_OK;g.index=1;g.advance=8*64;
    assert(eza_icon_features(glyph,width,8)==12);
    g.advance=9*64;assert(!eza_icon_features(glyph,width,8));
    g.advance=8*64;g.status=XFONT_MISSING;assert(!eza_icon_features(glyph,width,8));
    assert(!eza_icon_features(glyph,width,0));
    return 0;
}
