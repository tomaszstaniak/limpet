/* Session-start output check for the PTY frontend. Writes marked lines at
   once, after 1 s and after 3 s, and records every write result in
   RAM:early-output.txt (RAM:early-libc.txt for the libc build). Comparing
   that file with the screen tells lost bytes (a failed or short write) from
   bytes accepted but not shown.

   Default: plain DOS Write on Output(), one packet per line, no libc.
   -DEARLY_LIBC: write(STDOUT_FILENO) through posixc, and a terminal query
   (tcgetattr + TIOCGWINSZ) between the second and third phase, because
   libc output from a program that only printed was missing from the screen
   while DOS output was not (2026-09-27).
   With EARLY_LIBC also: -DEARLY_STDIO writes through stdio (fputs + fflush)
   instead of write(); -DEARLY_QUERY_FIRST runs the terminal query before the
   first phase, as the geometry probe does.
   -DEARLY_QUICK (either mode): only the immediate phase, then exit at once,
   as the probe does; the record file gets a "-quick" name.
   -DEARLY_EXIT_FLUSH (with EARLY_STDIO): no fflush, so stdio's flush at
   program exit is the only write, again as the probe does. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#if defined(EARLY_EXIT_FLUSH) && defined(EARLY_QUERY_FIRST)
#define RECORD_FILE "RAM:early-exitflush-query.txt"
#elif defined(EARLY_EXIT_FLUSH)
#define RECORD_FILE "RAM:early-exitflush.txt"
#elif defined(EARLY_QUICK)
#ifdef EARLY_LIBC
#define RECORD_FILE "RAM:early-libc-quick.txt"
#else
#define RECORD_FILE "RAM:early-dos-quick.txt"
#endif
#endif
#ifdef EARLY_LIBC
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#if defined(EARLY_QUICK) || defined(EARLY_EXIT_FLUSH)
#elif defined(EARLY_STDIO) && defined(EARLY_QUERY_FIRST)
#define RECORD_FILE "RAM:early-stdio-query.txt"
#elif defined(EARLY_STDIO)
#define RECORD_FILE "RAM:early-stdio.txt"
#elif defined(EARLY_QUERY_FIRST)
#define RECORD_FILE "RAM:early-query.txt"
#else
#define RECORD_FILE "RAM:early-libc.txt"
#endif
#elif !defined(EARLY_QUICK)
#define RECORD_FILE "RAM:early-output.txt"
#endif

static char record[2048];
static size_t used;

static void note(const char *text)
{
    used += snprintf(record + used, sizeof record - used, "%s", text);
}

static void line(const char *phase, int n)
{
    char text[64], result[128];
    long want, got, error;

    snprintf(text, sizeof text, "EARLY %s %02d\n", phase, n);
    want = (long)strlen(text);
#ifdef EARLY_LIBC
    errno = 0;
#if defined(EARLY_STDIO) && defined(EARLY_EXIT_FLUSH)
    /* Buffered only; "got" means accepted by stdio, not written. */
    got = fputs(text, stdout) >= 0 ? want : -1;
#elif defined(EARLY_STDIO)
    got = fputs(text, stdout) >= 0 && fflush(stdout) == 0 ? want : -1;
#else
    got = (long)write(STDOUT_FILENO, text, (size_t)want);
#endif
    error = got == want ? 0 : errno;
#else
    got = Write(Output(), (APTR)text, want);
    error = got == want ? 0 : IoErr();
#endif
    snprintf(result, sizeof result, "%s %02d want=%ld got=%ld error=%ld\n", phase, n, want, got, error);
    note(result);
}

static void phase(const char *name)
{
    int n;
    for (n = 1; n <= 5; n++) line(name, n);
}

#if defined(EARLY_LIBC) && (defined(EARLY_QUERY_FIRST) || !defined(EARLY_QUICK))
static void query(void)
{
    struct termios t;
    struct winsize ws;
    char result[128];
    int a = tcgetattr(STDIN_FILENO, &t), ae = a ? errno : 0;
    int g = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws), ge = g ? errno : 0;
    snprintf(result, sizeof result, "query tcgetattr=%d/%d winsize=%d/%d %ux%u\n",
             a, ae, g, ge, g ? 0u : (unsigned)ws.ws_col, g ? 0u : (unsigned)ws.ws_row);
    note(result);
}
#endif

int main(void)
{
    BPTR file;

#ifdef EARLY_QUERY_FIRST
    query();
#endif
    phase("immediate");
#ifndef EARLY_QUICK
    Delay(50);
    phase("after1s");
#if defined(EARLY_LIBC) && !defined(EARLY_QUERY_FIRST)
    query();
#endif
    Delay(100);
    phase("after3s");
#endif
    if (!(file = Open((CONST_STRPTR)RECORD_FILE, MODE_NEWFILE)))
        return RETURN_ERROR;
    Write(file, record, (LONG)used);
    Close(file);
    return RETURN_OK;
}
