#ifndef XTY_TERMIOS_H
#define XTY_TERMIOS_H
#include <termios.h>
#include "tty_control.h"
/* Experimental codec, not libc symbols. Returns an errno value (0 on success).
   Output is untouched on error. Virtual line profile: CS8/CREAD/CLOCAL,
   fixed nominal B38400; unsupported behavior is rejected. */
int xty_termios_decode(const XtyControl *,struct termios *);
int xty_termios_encode(const struct termios *,int,XtyControl *);
int xty_termios_flush(int,XtyControl *);
int xty_termios_flow(int,XtyControl *);
#endif
