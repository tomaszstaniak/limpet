#ifndef AROS_TTY_SIGNAL_H
#define AROS_TTY_SIGNAL_H
#include "tty_service.h"
#include "aros_break.h"
typedef struct {
    ArosBreak gate;
    TtyService *service;
    unsigned delivered, dropped;
} ArosTtySignal;
/* Private owner-only route, NOT a packet or libc ABI. Initially zeroed storage.
   Target must be known alive and launch-gated (or holding a live request) until
   open returns. No PID lookup/cached-pointer fallback. Service and route must
   outlive registration; close BEFORE stopping/disposing the service. */
TtyStatus aros_tty_signal_open(ArosTtySignal *,TtyService *,struct Task *);
void aros_tty_signal_close(ArosTtySignal *);
#endif
