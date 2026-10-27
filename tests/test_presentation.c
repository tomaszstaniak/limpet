#include "../src/xterm_presentation.h"
#include <assert.h>
#include <stdio.h>
static XtyControl request(void)
{ XtyControl c;memset(&c,0,sizeof c);c.version=1;c.operation=13;return c; }
int main(void)
{
    XtyControl c=request(),before;unsigned i;
    assert(XTY_GET_PRESENTATION==13 && sizeof c==64);
    assert(xterm_presentation_reply(&c,sizeof c,24,80,0));
    assert(c.version==1 && c.operation==13 && c.reserved[0]==XTY_PACKET_MAGIC && !c.reserved[1]);
    assert(c.values[0]==255 && c.values[1]==3 && c.values[2]==41 && c.values[3]==3);
    assert(c.values[4]==24 && c.values[5]==80);
    assert(c.values[6]==0);
    /* Input features (bit 32): what a raw-mode client may turn on. */
    assert(c.values[7]==(XTY_INPUT_BRACKETED_PASTE|XTY_INPUT_RESIZE_REPORT|XTY_INPUT_WHEEL_REPORT|XTY_INPUT_MODIFIED_ENTER));
    /* Features (bit 64, 0.3.0): OSC 0/2 window titles, OSC 8 links, and
       (0.8.0) the Kitty graphics subset. */
    assert(XTY_FEATURE_KITTY_GRAPHICS==16u);
    assert(c.values[8]==(XTY_FEATURE_TITLE|2u|XTY_FEATURE_KITTY_GRAPHICS));
    /* Text (bit 128, 0.8.0): UTF-8 decoded, wide cells, combining clusters. */
    assert(c.values[9]==(XTY_TEXT_UTF8|XTY_TEXT_WIDE|XTY_TEXT_COMBINING));
    for(i=10;i<12;++i) assert(!c.values[i]);
    c=request();assert(xterm_presentation_reply(&c,sizeof c,0,80,0));
    assert(c.values[0]==247 && !c.values[4] && !c.values[5]);
    c=request();assert(xterm_presentation_reply(&c,sizeof c,18,63,XTY_COLOR_PENS));
    assert(c.values[4]==18 && c.values[5]==63 && c.values[6]==XTY_COLOR_PENS);
    for(i=0;i<16;++i) {
        c=request();((uint32_t *)&c)[i]=99;before=c;
        assert(!xterm_presentation_reply(&c,sizeof c,24,80,0));
        assert(!memcmp(&c,&before,sizeof c));
    }
    c=request();before=c;
    assert(!xterm_presentation_reply(&c,sizeof c-1,24,80,0));
    assert(!memcmp(&c,&before,sizeof c));
    /* Identity (operation 14). */
    {
        XtyControl id;unsigned part[3];
        memset(&id,0,sizeof id);id.version=1;id.operation=XTY_GET_IDENTITY;
        assert(XTY_GET_IDENTITY==14);
        assert(xterm_identity_reply(&id,sizeof id,"Limpet","0.8.0"));
        assert(id.reserved[0]==XTY_PACKET_MAGIC && id.values[0]==3 &&
               id.values[1]==0 && id.values[2]==8 && id.values[3]==0);
        /* A test build or a default "0.0.0-dev" gives the name, no version. */
        {
            static const char *const unreleased[]={"0.8.0-test1","0.0.0-dev","x","1.2","1.2.3.4","123456.0.0",""};
            unsigned k;
            for(k=0;k<sizeof unreleased/sizeof unreleased[0];++k) {
                XtyControl t2;memset(&t2,0,sizeof t2);t2.version=1;t2.operation=XTY_GET_IDENTITY;
                assert(xterm_identity_reply(&t2,sizeof t2,"Limpet",unreleased[k]));
                assert(t2.values[0]==XTY_IDENTITY_KNOWN_NAME && !t2.values[1] && !t2.values[2] && !t2.values[3]);
            }
        }
        assert(!memcmp(&id.values[4],"Limpet\0\0",8));
        for(i=6;i<XTY_IDENTITY_NAME_BYTES;++i) assert(!((unsigned char *)&id.values[4])[i]);
        before=id;assert(!xterm_identity_reply(&id,sizeof id,"Limpet","1"));
        assert(!memcmp(&id,&before,sizeof id)); /* a reply is not a request */
        assert(xterm_version_parts("12.3.99999",part) && part[0]==12 && part[1]==3 && part[2]==99999);
        assert(!xterm_version_parts("12.3",part) && !xterm_version_parts("x",part));
        c=request();assert(!xterm_identity_reply(&c,sizeof c,"Limpet","1.0.0"));
    }
    puts("PRESENTATION PASS: wire layout, geometry, pen interpretation, title and Kitty features, text bits, identity, unknown fields, rejection preserves request");
    return 0;
}
