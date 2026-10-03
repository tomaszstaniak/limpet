#include "../src/terminal_input.h"
#include "../src/echo_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Wrong VT dispatch or modifier arithmetic must change these literal bytes. */
static void keys(void)
{
    static const struct { TerminalKey key; unsigned mod; const char *want; } cases[] = {
        {TERMINAL_KEY_ESCAPE, 0, "\033"}, {TERMINAL_KEY_ENTER, 0, "\r"},
        {TERMINAL_KEY_BACKSPACE, 0, "\177"}, {TERMINAL_KEY_TAB, 0, "\t"},
        {TERMINAL_KEY_TAB, TERMINAL_SHIFT, "\033[Z"},
        {TERMINAL_KEY_UP, 0, "\033[A"}, {TERMINAL_KEY_DOWN, 0, "\033[B"},
        {TERMINAL_KEY_RIGHT, 0, "\033[C"}, {TERMINAL_KEY_LEFT, 0, "\033[D"},
        {TERMINAL_KEY_HOME, 0, "\033[H"}, {TERMINAL_KEY_END, 0, "\033[F"},
        {TERMINAL_KEY_INSERT, 0, "\033[2~"}, {TERMINAL_KEY_DELETE, 0, "\033[3~"},
        {TERMINAL_KEY_PAGEUP, 0, "\033[5~"}, {TERMINAL_KEY_PAGEDOWN, 0, "\033[6~"},
        {TERMINAL_KEY_F1, 0, "\033OP"}, {TERMINAL_KEY_F2, 0, "\033OQ"},
        {TERMINAL_KEY_F3, 0, "\033OR"}, {TERMINAL_KEY_F4, 0, "\033OS"},
        {TERMINAL_KEY_F5, 0, "\033[15~"}, {TERMINAL_KEY_F6, 0, "\033[17~"},
        {TERMINAL_KEY_F7, 0, "\033[18~"}, {TERMINAL_KEY_F8, 0, "\033[19~"},
        {TERMINAL_KEY_F9, 0, "\033[20~"}, {TERMINAL_KEY_F10, 0, "\033[21~"},
        {TERMINAL_KEY_F11, 0, "\033[23~"}, {TERMINAL_KEY_F12, 0, "\033[24~"},
        {TERMINAL_KEY_LEFT, TERMINAL_CTRL, "\033[1;5D"},
        {TERMINAL_KEY_HOME, TERMINAL_SHIFT, "\033[1;2H"},
        {TERMINAL_KEY_F3, TERMINAL_ALT, "\033[1;3R"},
        {TERMINAL_KEY_DELETE, 7, "\033[3;8~"},
        {TERMINAL_KEY_TAB, 7, "\033[1;8Z"},
        {TERMINAL_KEY_ENTER, TERMINAL_ALT, "\033\r"}
    };
    unsigned char out[32];
    size_t i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        size_t n = strlen(cases[i].want);
        assert(terminal_encode_key(cases[i].key, cases[i].mod, out, sizeof out) == n);
        assert(memcmp(out, cases[i].want, n) == 0);
        memset(out, 0xa5, sizeof out);
        assert(terminal_encode_key(cases[i].key, cases[i].mod, out, n - 1) == 0);
        assert(out[0] == 0xa5);
    }
    assert(terminal_encode_key(TERMINAL_KEY_NONE, 0, out, sizeof out) == 0);
}

/* Catch truncated multi-byte output, double Ctrl mapping, and missing Meta. */
static void text(void)
{
    unsigned char out[16], latin[] = {'A', 0xe9, 0xff};
    const unsigned char control[] = {'a', 'Z', '[', ' ', '?', '2'};
    assert(terminal_encode_latin1(latin, sizeof latin, 0, out, sizeof out) == 5);
    assert(memcmp(out, "A\xc3\xa9\xc3\xbf", 5) == 0);
    memset(out, 0xa5, sizeof out);
    assert(terminal_encode_latin1(latin, sizeof latin, 0, out, 4) == 0);
    assert(out[0] == 0xa5);
    assert(terminal_encode_latin1(control, sizeof control, TERMINAL_CTRL, out, sizeof out) == 6);
    assert(memcmp(out, "\001\032\033\000\177\000", 6) == 0);
    assert(terminal_encode_latin1((const unsigned char *)"c", 1,
                                  TERMINAL_ALT | TERMINAL_CTRL, out, sizeof out) == 2);
    assert(memcmp(out, "\033\003", 2) == 0);
    assert(terminal_encode_latin1(NULL, 0, TERMINAL_ALT, out, sizeof out) == 0);
}

/* Expansion, ring wrap/backpressure and close/reset must preserve order. */
static void echo(void)
{
    EchoSession session;
    unsigned char out[ECHO_SESSION_CAPACITY + 10], fill[ECHO_SESSION_CAPACITY];
    const unsigned char input[] = "hi\r\177\010\033[D";
    const unsigned char want[] = "hi\r\n\b \b\b \b\033[D";
    size_t i;
    echo_session_init(&session);
    assert(echo_session_write(&session, input, sizeof input - 1) == sizeof input - 1);
    for (i = 0; i < sizeof want - 1; ++i) {
        assert(echo_session_read(&session, out, 1) == 1);
        assert(out[0] == want[i]);
    }
    assert(echo_session_read(&session, out, sizeof out) == 0);
    memset(fill, 'x', sizeof fill);
    assert(echo_session_write(&session, fill, sizeof fill - 1) == sizeof fill - 1);
    assert(echo_session_write(&session, (const unsigned char *)"\r", 1) == 0);
    assert(echo_session_write(&session, (const unsigned char *)"!\n", 2) == 1);
    assert(echo_session_read(&session, out, sizeof out) == ECHO_SESSION_CAPACITY);
    assert(memcmp(out, fill, sizeof fill - 1) == 0 && out[sizeof fill - 1] == '!');
    assert(echo_session_write(&session, fill, sizeof fill - 2) == sizeof fill - 2);
    assert(echo_session_write(&session, (const unsigned char *)"\177", 1) == 0);
    assert(echo_session_read(&session, out, 1) == 1);
    assert(echo_session_write(&session, (const unsigned char *)"\177", 1) == 1);
    assert(echo_session_read(&session, out, sizeof out) == ECHO_SESSION_CAPACITY);
    assert(memcmp(out + ECHO_SESSION_CAPACITY - 3, "\b \b", 3) == 0);
    assert(echo_session_write(&session, (const unsigned char *)"old", 3) == 3);
    echo_session_close(&session);
    assert(echo_session_read(&session, out, sizeof out) == 0);
    assert(echo_session_write(&session, input, sizeof input - 1) == 0);
    echo_session_init(&session);
    assert(echo_session_read(&session, out, sizeof out) == 0);
    assert(echo_session_write(&session, (const unsigned char *)"new", 3) == 3);
    assert(echo_session_read(&session, out, sizeof out) == 3);
    assert(memcmp(out, "new", 3) == 0);
}
int main(void)
{
    keys(); text(); echo();
    puts("input/echo tests passed");
    return 0;
}
