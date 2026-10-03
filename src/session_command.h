#ifndef XTERM_SESSION_COMMAND_H
#define XTERM_SESSION_COMMAND_H
#include <stddef.h>
/* Quote a literal executable path. Reject Shell substitutions rather than
 * silently interpreting installation-directory names as command language. */
int session_quote_path(const char *,char *,size_t);
#endif
