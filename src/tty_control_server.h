#ifndef XTY_CONTROL_SERVER_H
#define XTY_CONTROL_SERVER_H
#include "tty_control.h"
#include "tty_pair.h"
#include "tty_service.h"
/* Pure preparation: no mutation, admission, capability claim or completion. */
TtyStatus tty_control_prepare_async(TtyEnd,const void *,size_t,TtyOperation *,TtyConfig *);
TtyStatus tty_control_apply(TtyPair *,TtyEnd,void *,size_t);
#endif
