#ifndef XTERM_SESSION_RESET_H
#define XTERM_SESSION_RESET_H
/* Written into the screen when a PTY session ends: the modes a program
 * turned on must not outlive it, or the next program in the window gets
 * mouse reports or framed pastes it never asked for. */
#define XTERM_SESSION_RESET "\033[?1002l\033[?1000l\033[?1006l\033[?2004l\033[?2048l\033[?1049l\033[0m\033[?25h"
#endif
