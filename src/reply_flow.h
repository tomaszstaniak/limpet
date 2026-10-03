#ifndef XTERM_REPLY_FLOW_H
#define XTERM_REPLY_FLOW_H
#include "screen_model.h"
typedef struct {
    unsigned char bytes[XSCREEN_REPLY_CAPACITY];
    size_t length,offset;
} XReplyFlow;
/* Caller retains the unconsumed suffix. At most one response is pending.
 * Writer returns accepted bytes, possibly zero. No reentry into the model.
 * Clearing this object cancels an old input owner's pending response. */
size_t reply_flow_write(XReplyFlow *,XTermScreen *,const unsigned char *,size_t,
                       size_t (*)(void *,const unsigned char *,size_t),void *);
#endif
