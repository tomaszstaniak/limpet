#include "aros_tty_termios.h"
#include "aros_tty_client.h"
#include "tty_termios.h"
#include <proto/dos.h>
#include <errno.h>
#include <string.h>

static int failure(int error)
{ errno=error;return -1; }
static int transport_error(void)
{
    switch(IoErr()) {
    case ERROR_INVALID_LOCK:return failure(EBADF);
    case ERROR_ACTION_NOT_KNOWN:return failure(ENOTTY);
    case ERROR_OBJECT_IN_USE:return failure(EBUSY);
    case ERROR_BAD_NUMBER:return failure(EINVAL);
    case ERROR_BREAK:return failure(EINTR);
    case ERROR_NO_FREE_STORE:return failure(ENOMEM);
    default:return failure(EIO);
    }
}
static int exchange(BPTR handle,uint32_t capability,XtyControl *c)
{
    XtyControl caps;
    int blocking=c->operation==XTY_DRAIN || c->operation==XTY_DRAIN_CONFIG ||
        c->operation==XTY_DRAIN_CONFIG_DISCARD_INPUT;
    if(!handle) return failure(EBADF);
    /* NIL: is a valid DOS handle without a handler port; non-terminal
       handles must not be mistaken for broken PTY exports. */
    if(!IsInteractive(handle)) return failure(ENOTTY);
    memset(&caps,0,sizeof caps);caps.version=XTY_CONTROL_VERSION;caps.operation=XTY_CAPS;
    if(aros_tty_control(handle,&caps)!=DOSTRUE) return transport_error();
    if(blocking) capability|=XTY_CAP_CANCEL;
    if((caps.values[0]&capability)!=capability) return failure(ENOTSUP);
    if((blocking?aros_tty_control_break(handle,c):aros_tty_control(handle,c))!=DOSTRUE)
        return transport_error();
    return 0;
}
int aros_tty_tcgetattr(BPTR handle,struct termios *out)
{
    XtyControl c;int error;
    if(!out) return failure(EFAULT);
    memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;c.operation=XTY_GET_CONFIG;
    if(exchange(handle,XTY_CAP_GET_CONFIG,&c)) return -1;
    error=xty_termios_decode(&c,out);return error?failure(error):0;
}
int aros_tty_tcsetattr(BPTR handle,int action,const struct termios *t)
{
    XtyControl c,before;uint32_t capability;int error=xty_termios_encode(t,action,&c);
    if(error) return failure(error);
    capability=action==TCSANOW?XTY_CAP_SET_CONFIG:
        action==TCSADRAIN?XTY_CAP_DRAIN_CONFIG:XTY_CAP_DRAIN_CONFIG_DISCARD_INPUT;
    if((c.values[2]&(XTY_ECHOE|XTY_ECHOK|XTY_ECHONL|XTY_NOEOF)) || c.values[4] || c.values[5] || c.values[6])
        capability|=XTY_CAP_CANONICAL_EDIT;
    if(c.values[7] || c.values[8]) capability|=XTY_CAP_READ_TIMING;
    if((c.values[2]&(XTY_ISIG|XTY_NOFLSH)) || c.values[9]) capability|=XTY_CAP_VINTR;
    before=c;
    if(exchange(handle,capability,&c)) return -1;
    /* A malformed success cannot roll back the remote change, but must not
       be presented to the caller as a validated mode transition. */
    if(memcmp(c.values,before.values,sizeof c.values)) return failure(EIO);
    return 0;
}
static int empty_response(BPTR handle,uint32_t capability,XtyControl *c)
{
    unsigned i;
    if(exchange(handle,capability,c)) return -1;
    for(i=0;i<12;++i) if(c->values[i]) return failure(EIO);
    return 0;
}
int aros_tty_tcdrain(BPTR handle)
{
    XtyControl c;memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;c.operation=XTY_DRAIN;
    return empty_response(handle,XTY_CAP_DRAIN,&c);
}
int aros_tty_tcflush(BPTR handle,int selector)
{
    XtyControl c;int error=xty_termios_flush(selector,&c);
    return error?failure(error):empty_response(handle,XTY_CAP_FLUSH,&c);
}
int aros_tty_tcflow(BPTR handle,int action)
{
    XtyControl c;int error=xty_termios_flow(action,&c);
    return error?failure(error):empty_response(handle,XTY_CAP_OUTPUT_FLOW,&c);
}
