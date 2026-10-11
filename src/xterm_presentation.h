#ifndef XTERM_PRESENTATION_H
#define XTERM_PRESENTATION_H
#include "tty_control.h"
#include <string.h>
/* Only a frontend with an attached XTerm renderer may call this helper.
   Reject malformed requests without changing their storage. */
static inline int xterm_presentation_reply(void *block,size_t bytes,
                                            unsigned rows,unsigned columns,
                                            unsigned interpretation)
{
    XtyControl in,out;unsigned i;
    if(!block || bytes!=sizeof in) return 0;
    memcpy(&in,block,sizeof in);
    if(in.version!=XTY_CONTROL_VERSION || in.operation!=XTY_GET_PRESENTATION ||
       in.reserved[0] || in.reserved[1]) return 0;
    for(i=0;i<12;++i) if(in.values[i]) return 0;
    memset(&out,0,sizeof out);
    out.version=in.version;out.operation=in.operation;
    out.reserved[0]=XTY_PACKET_MAGIC;
    out.values[0]=XTY_PRESENT_KNOWN_COLORS|XTY_PRESENT_KNOWN_STYLES|XTY_PRESENT_KNOWN_LINES|
                  XTY_PRESENT_KNOWN_INTERPRETATION|XTY_PRESENT_KNOWN_INPUT|
                  XTY_PRESENT_KNOWN_FEATURES;
    out.values[7]=XTY_INPUT_BRACKETED_PASTE|XTY_INPUT_RESIZE_REPORT|XTY_INPUT_WHEEL_REPORT;
    out.values[6]=interpretation;
    out.values[8]=XTY_FEATURE_TITLE;
    out.values[1]=XTY_COLOR_RGB;out.values[2]=XTY_STYLE_BOLD_RESET;
    out.values[3]=XTY_LINE_CR|XTY_LINE_ERASE_EOL;
    if(rows && columns) {
        out.values[0]|=XTY_PRESENT_KNOWN_GEOMETRY;
        out.values[4]=rows;out.values[5]=columns;
    }
    memcpy(block,&out,sizeof out);return 1;
}
#endif
