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
    /* CSI > 4 ; 1 m: Shift+Enter told apart (modifyOtherKeys, Enter only). */
    feed("\033[>4;1m"); assert(s.modify_enter);
    feed("\033[>4m"); assert(!s.modify_enter);
    feed("\033[>4;2m"); assert(s.modify_enter);
    feed("\033[>4;0m"); assert(!s.modify_enter);
    feed("\033[>4;1m\033[>m"); assert(!s.modify_enter);              /* CSI > m resets all, as in xterm */
    {
        unsigned col = s.cursor_column;
        feed("\033[>4;1m\033[>5;1m\033[>c"); assert(s.modify_enter);  /* other CSI > ignored */
        feed("\033[?4;1m"); assert(s.modify_enter);                    /* not the same sequence */
        assert(s.cursor_column == col);                                /* nothing of them printed */
    }
    feed(XTERM_SESSION_RESET); assert(!s.modify_enter);
    feed("\033[>4;1m"); screen_reset_input_modes(&s); assert(!s.modify_enter);
    /* Native console back in cooked mode: every input report is off. */
    feed("\033[?1002;1006;2004;2048;2100h");
    assert(s.mouse_tracking == 1002 && s.mouse_sgr && s.bracketed_paste && s.resize_report && s.wheel_report);
    {
        unsigned generation = s.mouse_generation;
        screen_reset_input_modes(&s);
        assert(!s.mouse_tracking && !s.mouse_sgr && !s.bracketed_paste && !s.resize_report && !s.wheel_report);
        assert(s.mouse_generation != generation);  /* pending mouse releases are dropped */
    }
    puts("bracketed paste mode tests passed");
    return 0;
}
