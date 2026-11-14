#ifndef XTERM_SSH_PENS_H
#define XTERM_SSH_PENS_H
#include <dos/dos.h>
/* 1 when the terminal behind output said that SGR 30..37 and 40..47 select
 * screen pens; 0 when it said otherwise or did not answer. */
int xt_ssh_output_uses_pens(BPTR output);
#endif
