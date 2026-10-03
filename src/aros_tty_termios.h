#ifndef AROS_TTY_TERMIOS_H
#define AROS_TTY_TERMIOS_H
#include <dos/dos.h>
#include <termios.h>
/* Experimental private PTY client. BPTR is a live caller-owned DOS FileHandle,
   NOT a libc fd. Returns 0/-1 with errno. No global libc interposition.
   Draining controls support Ctrl-C cancellation on a capable private handler.
   Immediate queries use DoPkt; this does not implement POSIX signal dispatch.
   Caller serializes handle use and keeps it live through the whole call. */
int aros_tty_tcgetattr(BPTR,struct termios *);
int aros_tty_tcsetattr(BPTR,int,const struct termios *);
int aros_tty_tcdrain(BPTR);
int aros_tty_tcflush(BPTR,int);
int aros_tty_tcflow(BPTR,int);
#endif
