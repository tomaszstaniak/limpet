#include "tty_termios.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

static int virtual_profile(struct termios *t)
{
    memset(t,0,sizeof *t);t->c_cflag=CS8|CREAD|CLOCAL;
    memset(t->c_cc,(unsigned char)_POSIX_VDISABLE,sizeof t->c_cc);
    t->c_cc[VMIN]=1;t->c_cc[VTIME]=0;
    /* Nominal metadata only: no UART speed, hangup-on-B0 or modem control. */
    return cfsetispeed(t,B38400) || cfsetospeed(t,B38400)?EIO:0;
}
int xty_termios_decode(const XtyControl *c,struct termios *out)
{
    struct termios t;unsigned i;int error;
    if(!c || !out) return EFAULT;
    if(c->version!=XTY_CONTROL_VERSION || c->operation!=XTY_GET_CONFIG ||
       c->reserved[0]!=XTY_PACKET_MAGIC || c->reserved[1] ||
       (c->values[0]&~(XTY_ICRNL|XTY_IGNCR|XTY_INLCR)) ||
       (c->values[1]&~(XTY_OPOST|XTY_ONLCR)) ||
       (c->values[2]&~XTY_LOCAL_MASK) || c->values[3]>255 ||
       (c->values[3]=='\n' && !(c->values[2]&XTY_NOEOF))) return EIO;
    for(i=4;i<7;++i) if(c->values[i]>256) return EIO;
    if(c->values[7]>256 || c->values[8]>255) return EIO;
    if(c->values[9]>256) return EIO;
    for(i=10;i<12;++i) if(c->values[i]) return EIO;
    if(c->values[9] && c->values[9]-1==(unsigned char)_POSIX_VDISABLE) return ENOTSUP;
    if(!(c->values[2]&XTY_NOEOF) && c->values[3]==(unsigned char)_POSIX_VDISABLE) return ENOTSUP;
    for(i=4;i<7;++i) if(c->values[i] && c->values[i]-1==(unsigned char)_POSIX_VDISABLE) return ENOTSUP;
    error=virtual_profile(&t);if(error) return error;
    t.c_iflag=((c->values[0]&XTY_ICRNL)?ICRNL:0)|
        ((c->values[0]&XTY_IGNCR)?IGNCR:0)|((c->values[0]&XTY_INLCR)?INLCR:0);
    t.c_oflag=((c->values[1]&XTY_OPOST)?OPOST:0)|((c->values[1]&XTY_ONLCR)?ONLCR:0);
    t.c_lflag=((c->values[2]&XTY_CANON)?ICANON:0)|((c->values[2]&XTY_ECHO)?ECHO:0)|
        ((c->values[2]&XTY_ECHOE)?ECHOE:0)|((c->values[2]&XTY_ECHOK)?ECHOK:0)|
        ((c->values[2]&XTY_ECHONL)?ECHONL:0)|
        ((c->values[2]&XTY_ISIG)?ISIG:0)|((c->values[2]&XTY_NOFLSH)?NOFLSH:0);
    t.c_cc[VINTR]=c->values[9]?(cc_t)(c->values[9]-1):(cc_t)_POSIX_VDISABLE;
    t.c_cc[VEOF]=(c->values[2]&XTY_NOEOF)?(cc_t)_POSIX_VDISABLE:(cc_t)c->values[3];
    t.c_cc[VERASE]=c->values[4]?(cc_t)(c->values[4]-1):(cc_t)_POSIX_VDISABLE;
    t.c_cc[VKILL]=c->values[5]?(cc_t)(c->values[5]-1):(cc_t)_POSIX_VDISABLE;
    t.c_cc[VEOL]=c->values[6]?(cc_t)(c->values[6]-1):(cc_t)_POSIX_VDISABLE;
    t.c_cc[VMIN]=c->values[7]?(cc_t)(c->values[7]-1):1;
    t.c_cc[VTIME]=(cc_t)c->values[8];
    *out=t;return 0;
}
int xty_termios_encode(const struct termios *t,int action,XtyControl *out)
{
    XtyControl c;struct termios profile;unsigned i;int error;
    if(!t || !out) return EFAULT;
    memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;
    switch(action) {
    case TCSANOW:c.operation=XTY_SET_CONFIG;break;
    case TCSADRAIN:c.operation=XTY_DRAIN_CONFIG;break;
    case TCSAFLUSH:c.operation=XTY_DRAIN_CONFIG_DISCARD_INPUT;break;
    default:return EINVAL;
    }
    if(t->c_cc[VEOF]=='\n') return EINVAL;
    error=virtual_profile(&profile);if(error) return error;
    if((t->c_iflag&~(tcflag_t)(ICRNL|IGNCR|INLCR)) ||
       (t->c_oflag&~(tcflag_t)(OPOST|ONLCR)) ||
       (t->c_lflag&~(tcflag_t)(ICANON|ECHO|ECHOE|ECHOK|ECHONL|ISIG|NOFLSH)) || t->c_cflag!=profile.c_cflag ||
       cfgetispeed(t)!=B38400 || cfgetospeed(t)!=B38400) return ENOTSUP;
    for(i=0;i<NCCS;++i)
        if(i!=VEOF && i!=VERASE && i!=VKILL && i!=VEOL && i!=VMIN && i!=VTIME && i!=VINTR &&
           t->c_cc[i]!=(cc_t)_POSIX_VDISABLE) return ENOTSUP;
    c.values[0]=((t->c_iflag&ICRNL)?XTY_ICRNL:0)|
        ((t->c_iflag&IGNCR)?XTY_IGNCR:0)|((t->c_iflag&INLCR)?XTY_INLCR:0);
    c.values[1]=((t->c_oflag&OPOST)?XTY_OPOST:0)|((t->c_oflag&ONLCR)?XTY_ONLCR:0);
    c.values[2]=((t->c_lflag&ICANON)?XTY_CANON:0)|((t->c_lflag&ECHO)?XTY_ECHO:0)|
        ((t->c_lflag&ECHOE)?XTY_ECHOE:0)|((t->c_lflag&ECHOK)?XTY_ECHOK:0)|
        ((t->c_lflag&ECHONL)?XTY_ECHONL:0)|
        ((t->c_lflag&ISIG)?XTY_ISIG:0)|((t->c_lflag&NOFLSH)?XTY_NOFLSH:0)|
        ((t->c_cc[VEOF]==(cc_t)_POSIX_VDISABLE)?XTY_NOEOF:0);
    c.values[3]=(c.values[2]&XTY_NOEOF)?0:t->c_cc[VEOF];
    c.values[4]=t->c_cc[VERASE]==(cc_t)_POSIX_VDISABLE?0:(unsigned)t->c_cc[VERASE]+1;
    c.values[5]=t->c_cc[VKILL]==(cc_t)_POSIX_VDISABLE?0:(unsigned)t->c_cc[VKILL]+1;
    c.values[6]=t->c_cc[VEOL]==(cc_t)_POSIX_VDISABLE?0:(unsigned)t->c_cc[VEOL]+1;
    c.values[7]=t->c_cc[VMIN]==1?0:(unsigned)t->c_cc[VMIN]+1;
    c.values[8]=t->c_cc[VTIME];
    c.values[9]=t->c_cc[VINTR]==(cc_t)_POSIX_VDISABLE?0:(unsigned)t->c_cc[VINTR]+1;
    *out=c;return 0;
}
int xty_termios_flow(int action,XtyControl *out)
{
    XtyControl c;if(!out) return EFAULT;
    memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;c.operation=XTY_OUTPUT_FLOW;
    switch(action) {
    case TCOOFF:c.values[0]=XTY_OUTPUT_STOP;break;
    case TCOON:c.values[0]=XTY_OUTPUT_START;break;
    case TCIOFF:case TCION:return ENOTSUP;
    default:return EINVAL;
    }
    *out=c;return 0;
}
int xty_termios_flush(int selector,XtyControl *out)
{
    XtyControl c;
    if(!out) return EFAULT;
    memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;c.operation=XTY_FLUSH;
    switch(selector) {
    case TCIFLUSH:c.values[0]=XTY_FLUSH_INPUT;break;
    case TCOFLUSH:c.values[0]=XTY_FLUSH_OUTPUT;break;
    case TCIOFLUSH:c.values[0]=XTY_FLUSH_INPUT|XTY_FLUSH_OUTPUT;break;
    default:return EINVAL;
    }
    *out=c;return 0;
}
