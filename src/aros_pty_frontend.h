#ifndef AROS_PTY_FRONTEND_H
#define AROS_PTY_FRONTEND_H
#include "aros_session.h"
/* Experimental single-window adapter. All calls run on the GUI process.
   Reuses the existing GUI queue/status contract, not its DOS line discipline. */
int aros_pty_start(ArosSession *,const char *executable);
int aros_pty_active(const ArosSession *);
ULONG aros_pty_signals(const ArosSession *);
void aros_pty_poll(ArosSession *);
size_t aros_pty_write(ArosSession *,const unsigned char *,size_t);
size_t aros_pty_read(ArosSession *,unsigned char *,size_t);
void aros_pty_eof(ArosSession *);
void aros_pty_stop(ArosSession *);
int aros_pty_dispose(ArosSession *);
/* Most output bytes moved from the PTY per poll before the GUI runs again
   (at least one 1024-byte read). Bounded so a producer cannot starve input. */
void aros_pty_set_receive_budget(size_t);
#endif
