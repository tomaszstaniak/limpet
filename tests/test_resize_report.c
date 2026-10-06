#include "../src/screen_model.h"
#include "../src/resize_report.h"
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
    char out[64];
    size_t n;
    assert(!screen_init(&s, cells, CAP, 24, 80));
    assert(!s.resize_report);
    feed("\033[?2048h"); assert(s.resize_report);
    feed("\033[?2048l"); assert(!s.resize_report);
    feed("\033[?2048h"); feed(XTERM_SESSION_RESET); assert(!s.resize_report);
    n = resize_report(out, sizeof out, 30, 100, 480, 800);
    assert(n == strlen("\033[48;30;100;480;800t") && !memcmp(out, "\033[48;30;100;480;800t", n));
    assert(resize_report(out, 8, 30, 100, 480, 800) == 0);            /* too small: nothing written */
    puts("ok resize report");
    return 0;
}
