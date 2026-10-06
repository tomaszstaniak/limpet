/* DEC mode 2004 (bracketed paste): set and cleared by the client, and
 * cleared by the reset Limpet writes when a PTY session ends, so the next
 * program in the window does not receive framed pastes it never asked for. */
#include "../src/screen_model.h"
#include "../src/session_reset.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CAP (24 * 80)
static XTermScreen s;
static XTermCell cells[CAP];
static void feed(const char *t) { screen_write(&s, (const unsigned char *)t, strlen(t)); }

int main(void)
{
    assert(!screen_init(&s, cells, CAP, 24, 80));
    assert(!s.bracketed_paste);
    feed("\033[?2004h"); assert(s.bracketed_paste);
    feed("\033[?2004l"); assert(!s.bracketed_paste);
    feed("\033[?1000;2004h"); assert(s.bracketed_paste && s.mouse_tracking == 1000);
    feed(XTERM_SESSION_RESET);
    assert(!s.bracketed_paste && !s.mouse_tracking);
    puts("bracketed paste mode tests passed");
    return 0;
}
