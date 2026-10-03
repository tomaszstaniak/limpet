#include "tty_control_server.h"
#include <string.h>
static TtyStatus decode_config(const XtyControl *in,TtyConfig *c)
{
    unsigned i;
    for(i=10;i<12;++i) if(in->values[i]) return TTY_INVALID;
    if(in->values[9]>256) return TTY_INVALID;
    if(in->values[7]>256 || in->values[8]>255) return TTY_INVALID;
    if(in->values[3]>255 || in->values[4]>256 || in->values[5]>256 || in->values[6]>256) return TTY_INVALID;
    if((in->values[0]&~(XTY_ICRNL|XTY_IGNCR|XTY_INLCR)) ||
       (in->values[1]&~(XTY_OPOST|XTY_ONLCR)) ||
       (in->values[2]&~XTY_LOCAL_MASK)) return TTY_UNSUPPORTED;
    memset(c,0,sizeof *c);c->version=TTY_PAIR_VERSION;
    c->input_flags=((in->values[0]&XTY_ICRNL)?TTY_ICRNL:0)|
        ((in->values[0]&XTY_IGNCR)?TTY_IGNCR:0)|((in->values[0]&XTY_INLCR)?TTY_INLCR:0);
    c->output_flags=((in->values[1]&XTY_OPOST)?TTY_OPOST:0)|((in->values[1]&XTY_ONLCR)?TTY_ONLCR:0);
    c->local_flags=in->values[2]; /* Private wire/core flags deliberately match. */
    c->eof=(unsigned char)in->values[3];
    c->erase=(uint16_t)in->values[4];c->kill=(uint16_t)in->values[5];c->eol=(uint16_t)in->values[6];
    c->vmin=in->values[7]?(unsigned char)(in->values[7]-1):1;
    c->vtime=(unsigned char)in->values[8];
    c->intr=(uint16_t)in->values[9];
    return tty_config_validate(c);
}
TtyStatus tty_control_prepare_async(TtyEnd end,const void *block,size_t size,
                                   TtyOperation *operation,TtyConfig *config)
{
    XtyControl in;TtyConfig c;TtyOperation op;TtyStatus status;unsigned i;
    if(!block || size!=sizeof in || !operation || !config ||
       (end!=TTY_MASTER && end!=TTY_SLAVE)) return TTY_INVALID;
    memcpy(&in,block,sizeof in);
    if(in.version!=XTY_CONTROL_VERSION) return TTY_UNSUPPORTED;
    if(in.reserved[0] || in.reserved[1]) return TTY_INVALID;
    if(end!=TTY_SLAVE) return TTY_UNSUPPORTED;
    memset(&c,0,sizeof c);
    switch(in.operation) {
    case XTY_DRAIN:
        for(i=0;i<12;++i) if(in.values[i]) return TTY_INVALID;
        op=TTY_REQUEST_DRAIN;break;
    case XTY_DRAIN_CONFIG: case XTY_DRAIN_CONFIG_DISCARD_INPUT:
        status=decode_config(&in,&c);if(status!=TTY_OK) return status;
        op=in.operation==XTY_DRAIN_CONFIG?TTY_REQUEST_DRAIN_CONFIG:TTY_REQUEST_DRAIN_CONFIG_DISCARD;
        break;
    default: return TTY_UNSUPPORTED;
    }
    *operation=op;*config=c;return TTY_OK;
}
TtyStatus tty_control_apply(TtyPair *pair,TtyEnd end,void *block,size_t size)
{
    XtyControl in,out;TtyStatus status=TTY_OK;unsigned i;
    if(!pair || !block || size!=sizeof in || (end!=TTY_MASTER && end!=TTY_SLAVE))
        return TTY_INVALID;
    if(!pair->refs[end]) return TTY_CLOSED;
    memcpy(&in,block,sizeof in);
    if(in.version!=XTY_CONTROL_VERSION) return TTY_UNSUPPORTED;
    if(in.reserved[0] || in.reserved[1]) return TTY_INVALID;
    memset(&out,0,sizeof out);out.version=in.version;out.operation=in.operation;
    if(in.operation==XTY_SET_GEOMETRY)
        for(i=4;i<12;++i) if(in.values[i]) return TTY_INVALID;
    switch(in.operation) {
    case XTY_CAPS:
        out.values[0]=XTY_CAP_GET_CONFIG|XTY_CAP_GET_GEOMETRY|XTY_CAP_SET_GEOMETRY|XTY_CAP_GET_READ_STATE;
        if(end==TTY_SLAVE) out.values[0]|=XTY_CAP_SET_CONFIG|XTY_CAP_FLUSH|
            XTY_CAP_SET_CONFIG_DISCARD_INPUT|XTY_CAP_CANONICAL_EDIT|XTY_CAP_OUTPUT_FLOW;
        out.values[1]=XTY_ICRNL|XTY_IGNCR|XTY_INLCR;out.values[2]=XTY_OPOST|XTY_ONLCR;
        out.values[3]=XTY_LOCAL_MASK&~(XTY_ISIG|XTY_NOFLSH);out.values[4]=255;out.values[5]=65535;
        break;
    case XTY_SET_CONFIG_DISCARD_INPUT:
    case XTY_SET_CONFIG: {
        TtyConfig c;
        if(end!=TTY_SLAVE) return TTY_UNSUPPORTED;
        status=decode_config(&in,&c);if(status!=TTY_OK) return status;
        status=in.operation==XTY_SET_CONFIG_DISCARD_INPUT?
            tty_pair_configure_discard_input(pair,&c):tty_pair_configure(pair,&c);
        if(status!=TTY_OK) return status;
        /* Fall through to return the canonical shared snapshot. */
    }
    /* fall through */
    case XTY_GET_CONFIG: {
        TtyConfig c;
        status=tty_pair_get_config(pair,TTY_PAIR_VERSION,&c);
        if(status!=TTY_OK) return status;
        out.values[0]=((c.input_flags&TTY_ICRNL)?XTY_ICRNL:0)|
            ((c.input_flags&TTY_IGNCR)?XTY_IGNCR:0)|((c.input_flags&TTY_INLCR)?XTY_INLCR:0);
        out.values[1]=((c.output_flags&TTY_OPOST)?XTY_OPOST:0)|((c.output_flags&TTY_ONLCR)?XTY_ONLCR:0);
        out.values[2]=c.local_flags;
        out.values[3]=c.eof;out.values[4]=c.erase;out.values[5]=c.kill;out.values[6]=c.eol;
        out.values[7]=c.vmin==1?0:(unsigned)c.vmin+1;out.values[8]=c.vtime;
        out.values[9]=c.intr;break;
    }
    case XTY_SET_GEOMETRY: {
        TtyGeometry g={TTY_PAIR_VERSION,in.values[0],in.values[1],in.values[2],in.values[3]};
        status=tty_pair_set_geometry(pair,&g);
        if(status!=TTY_OK) return status;
    }
    /* fall through */
    case XTY_GET_GEOMETRY: {
        TtyGeometry g;
        status=tty_pair_get_geometry(pair,TTY_PAIR_VERSION,&g);
        if(status!=TTY_OK) return status;
        out.values[0]=g.rows;out.values[1]=g.columns;out.values[2]=g.xpixel;out.values[3]=g.ypixel;
        break;
    }
    case XTY_OUTPUT_FLOW:
        if(end!=TTY_SLAVE) return TTY_UNSUPPORTED;
        for(i=1;i<12;++i) if(in.values[i]) return TTY_INVALID;
        if(in.values[0]!=XTY_OUTPUT_STOP && in.values[0]!=XTY_OUTPUT_START) return TTY_INVALID;
        status=tty_pair_output_flow(pair,in.values[0]==XTY_OUTPUT_STOP);
        if(status!=TTY_OK) return status;
        break;
    case XTY_FLUSH:
        if(end!=TTY_SLAVE) return TTY_UNSUPPORTED;
        for(i=1;i<12;++i) if(in.values[i]) return TTY_INVALID;
        if(!in.values[0] || (in.values[0]&~(XTY_FLUSH_INPUT|XTY_FLUSH_OUTPUT)))
            return TTY_INVALID;
        status=tty_pair_flush(pair,
            ((in.values[0]&XTY_FLUSH_INPUT)?TTY_FLUSH_INPUT:0)|
            ((in.values[0]&XTY_FLUSH_OUTPUT)?TTY_FLUSH_OUTPUT:0));
        if(status!=TTY_OK) return status;
        break;
    case XTY_GET_READ_STATE:
        switch(tty_pair_read_status(pair,end)) {
        case TTY_AGAIN: out.values[0]=XTY_READ_WAIT;break;
        case TTY_OK: out.values[0]=XTY_READ_DATA;break;
        case TTY_EOF: out.values[0]=XTY_READ_EOF;break;
        default: return TTY_INVALID; /* Live endpoints were validated above. */
        }
        break;
    default: return TTY_UNSUPPORTED;
    }
    out.reserved[0]=XTY_PACKET_MAGIC; /* Response discriminator, zero on input. */
    memcpy(block,&out,sizeof out);return TTY_OK;
}
