#ifndef AROS_TTY_FD_BRIDGE_H
#define AROS_TTY_FD_BRIDGE_H
#include <dos/dos.h>
/* Internal experimental linker bridge. Caller serializes descriptor lifetime.
   1: recognized private PTY; 0: use legacy libc; -1: errno failure. */
int aros_tty_fd_resolve(int fd,BPTR *out);
#endif
