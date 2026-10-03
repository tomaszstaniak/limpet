#ifndef XTERM_STATUS_H
#define XTERM_STATUS_H
#include <stdint.h>

/* Project-private, opt-in DOS packet; see docs/ssh-stream-contract.md.
 * Arg1: input fh_Arg1, Arg2: XTermStatus*, Arg3: sizeof(XTermStatus).
 * Caller initializes version and reserved. No pointer is retained. */
#define XTERM_ACTION_QUERY 0x58545101L
#define XTERM_STATUS_VERSION 1
typedef struct {
    uint32_t version,available,eof,columns,rows,reserved;
} XTermStatus;
#endif
