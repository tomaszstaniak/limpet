#ifndef XTERM_SSH_STREAM_H
#define XTERM_SSH_STREAM_H
#include <libssh/libssh.h>
void xt_ssh_dimensions(unsigned *columns,unsigned *rows);
/* NULL command requests an interactive PTY and shell. */
int xt_ssh_stream(ssh_session session,ssh_channel channel,const char *command);
#endif
