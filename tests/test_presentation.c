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
    assert(c.values[0]==127 && c.values[1]==3 && c.values[2]==1 && c.values[3]==3);
    assert(c.values[4]==24 && c.values[5]==80);
    assert(c.values[6]==0);
    /* Input features (bit 32): what a raw-mode client may turn on. */
    assert(c.values[7]==(XTY_INPUT_BRACKETED_PASTE|XTY_INPUT_RESIZE_REPORT|XTY_INPUT_WHEEL_REPORT|XTY_INPUT_MODIFIED_ENTER));
    /* Features (bit 64, 0.3.0): OSC 0/2 window titles. */
    assert(c.values[8]==XTY_FEATURE_TITLE);
    for(i=9;i<12;++i) assert(!c.values[i]);
    c=request();assert(xterm_presentation_reply(&c,sizeof c,0,80,0));
    assert(c.values[0]==119 && !c.values[4] && !c.values[5]);
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
    puts("PRESENTATION PASS: wire layout, geometry, pen interpretation, title feature, unknown fields, rejection preserves request");
    return 0;
}
