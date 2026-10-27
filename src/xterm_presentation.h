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
    out.values[7]=XTY_INPUT_BRACKETED_PASTE|XTY_INPUT_RESIZE_REPORT|XTY_INPUT_WHEEL_REPORT|XTY_INPUT_MODIFIED_ENTER;
    out.values[6]=interpretation;
    out.values[8]=XTY_FEATURE_TITLE|XTY_FEATURE_OSC8;
    out.values[1]=XTY_COLOR_RGB;out.values[2]=XTY_STYLE_BOLD_RESET|XTY_STYLE_UNDERLINE_RESET|XTY_STYLE_INVERSE_RESET;
    out.values[3]=XTY_LINE_CR|XTY_LINE_ERASE_EOL;
    /* The renderer decodes UTF-8, draws wide characters in two cells and
       joins combining clusters (screen_model.c, utf8proc widths). */
    out.values[0]|=XTY_PRESENT_KNOWN_TEXT;
    out.values[9]=XTY_TEXT_UTF8|XTY_TEXT_WIDE|XTY_TEXT_COMBINING;
    if(rows && columns) {
        out.values[0]|=XTY_PRESENT_KNOWN_GEOMETRY;
        out.values[4]=rows;out.values[5]=columns;
    }
    memcpy(block,&out,sizeof out);return 1;
}
/* "0.8.0", "0.8.0-test1": the leading numbers; 0 for a missing part. */
static inline void xterm_version_parts(const char *v,unsigned part[3])
{
    unsigned i;
    for(i=0;i<3;++i) {
        part[i]=0;
        while(v && *v>='0' && *v<='9') part[i]=part[i]*10+(unsigned)(*v++-'0');
        if(v && *v=='.') ++v; else v=NULL;
    }
}
#ifndef LIMPET_VERSION
#define LIMPET_VERSION "0.0.0-dev"
#endif
#define XTERM_IDENTITY_NAME "Limpet"
/* Operation 14 (tty_control.h). name: printable ASCII, 1..31 bytes. */
static inline int xterm_identity_reply(void *block,size_t bytes,const char *name,
                                        const char *version)
{
    XtyControl in,out;unsigned i,part[3];size_t n=name?strlen(name):0;
    if(!block || bytes!=sizeof in || !n || n>=XTY_IDENTITY_NAME_BYTES) return 0;
    memcpy(&in,block,sizeof in);
    if(in.version!=XTY_CONTROL_VERSION || in.operation!=XTY_GET_IDENTITY ||
       in.reserved[0] || in.reserved[1]) return 0;
    for(i=0;i<12;++i) if(in.values[i]) return 0;
    for(i=0;i<n;++i) if(name[i]<0x20 || name[i]>0x7e) return 0;
    memset(&out,0,sizeof out);
    out.version=in.version;out.operation=in.operation;
    out.reserved[0]=XTY_PACKET_MAGIC;
    out.values[0]=XTY_IDENTITY_KNOWN_NAME;
    memcpy(&out.values[4],name,n);
    if(version) {
        xterm_version_parts(version,part);
        out.values[0]|=XTY_IDENTITY_KNOWN_VERSION;
        out.values[1]=part[0];out.values[2]=part[1];out.values[3]=part[2];
    }
    memcpy(block,&out,sizeof out);return 1;
}
#endif
