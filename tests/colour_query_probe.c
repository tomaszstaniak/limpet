/* Guest probe: asks the console it runs in for its colours, as a program
 * that adapts to the terminal does, and writes the answers to a log.
 *     colour-query-probe LOG
 * Queries: OSC 10, 11, 12 and OSC 4;1 with "?" (BEL and ST forms), and
 * CSI ? 996 n (dark or light). Each line of LOG is the query's name and the
 * answer with control bytes shown as \e, \a and \\, or "no answer". A
 * console that does not know a query answers nothing; nothing is drawn. */
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <stdio.h>
#include <string.h>

static void ask(BPTR log, const char *name, const char *query)
{
    char answer[96], shown[320];
    LONG n = 0, i;
    size_t at = 0;
    Write(Output(), (APTR)query, (LONG)strlen(query));
    /* An answer comes at once; half a second covers a slow emulator. */
    while (n < (LONG)sizeof answer - 1 && WaitForChar(Input(), 500000)) {
        LONG got = Read(Input(), answer + n, 1);
        if (got <= 0) break;
        n += got;
        /* BEL, or ST's backslash, or a CSI answer's final letter ends it. */
        if (answer[n - 1] == 7 || (n > 1 && answer[n - 2] == 27 && answer[n - 1] == '\\') ||
            (answer[0] == 27 && n > 2 && answer[1] == '[' && answer[n - 1] >= '@' && answer[n - 1] <= '~')) break;
    }
    for (i = 0; i < n && at < sizeof shown - 4; ++i) {
        unsigned char c = (unsigned char)answer[i];
        if (c == 27) { shown[at++] = '\\'; shown[at++] = 'e'; }
        else if (c == 7) { shown[at++] = '\\'; shown[at++] = 'a'; }
        else if (c < 32 || c > 126) at += (size_t)snprintf(shown + at, sizeof shown - at, "\\x%02x", c);
        else shown[at++] = (char)c;
    }
    shown[at] = 0;
    FPrintf(log, (CONST_STRPTR)"%s %s\n", (IPTR)name, (IPTR)(n ? shown : "no answer"));
}

int main(int argc, char **argv)
{
    BPTR log;
    if (argc < 2) { PutStr((CONST_STRPTR)"Usage: colour-query-probe LOG\n"); return 20; }
    if (!IsInteractive(Input())) { PutStr((CONST_STRPTR)"colour-query-probe: not on a console\n"); return 10; }
    log = Open((CONST_STRPTR)argv[1], MODE_NEWFILE);
    if (!log) return 20;
    SetMode(Input(), 1);
    ask(log, "foreground-bel", "\033]10;?\007");
    ask(log, "background-bel", "\033]11;?\007");
    ask(log, "background-st", "\033]11;?\033\\");
    ask(log, "cursor-bel", "\033]12;?\007");
    ask(log, "palette1-bel", "\033]4;1;?\007");
    ask(log, "palette15-st", "\033]4;15;?\033\\");
    ask(log, "palette16-bel", "\033]4;16;?\007");
    ask(log, "scheme", "\033[?996n");
    SetMode(Input(), 0);
    Close(log);
    PutStr((CONST_STRPTR)"colour-query-probe: answers written\n");
    return 0;
}
