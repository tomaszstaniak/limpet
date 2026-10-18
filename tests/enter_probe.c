/* enter-probe: what the Enter keys send in raw mode, as bytes in a file.
 *
 *   enter-probe on|off LOG
 *
 * "on" turns the Shift+Enter mode on (CSI > 4 ; 1 m) first, "off" leaves it
 * off. Each key press is one line of hex in LOG (and on the screen). The
 * key n sends CSI > 4 n (the mode off, as in xterm), m sends CSI > 4 ; 1 m
 * again; q ends. Raw mode is undone on every way out. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

static int next(unsigned char *c, LONG microseconds)
{
    return WaitForChar(Input(), microseconds) && Read(Input(), c, 1) == 1;
}

static void say(FILE *log, const char *text)
{
    printf("%s\r\n", text);
    fflush(stdout);
    fprintf(log, "%s\n", text);
    fflush(log);
}

int main(int argc, char **argv)
{
    FILE *log;
    unsigned char c;
    int on;
    if (argc != 3 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
        puts("usage: enter-probe on|off LOG");
        return RETURN_ERROR;
    }
    on = !strcmp(argv[1], "on");
    if (!(log = fopen(argv[2], "w"))) { puts("cannot open the log"); return RETURN_ERROR; }
    if (!SetMode(Input(), 1)) { puts("no raw mode"); fclose(log); return RETURN_ERROR; }
    if (on) { fputs("\033[>4;1m", stdout); fflush(stdout); }
    say(log, on ? "mode on" : "mode off");
    for (;;) {
        char line[160];
        size_t n = 0;
        if (!next(&c, 120000000)) { say(log, "timeout"); break; }
        if (c == 'q') { say(log, "q"); break; }
        if (c == 'n') { fputs("\033[>4n", stdout); fflush(stdout); say(log, "sent CSI > 4 n"); continue; }
        if (c == 'm') { fputs("\033[>4;1m", stdout); fflush(stdout); say(log, "sent CSI > 4 ; 1 m"); continue; }
        /* One key: the first byte and whatever follows it within 50 ms. */
        n += (size_t)snprintf(line + n, sizeof line - n, "key:");
        do n += (size_t)snprintf(line + n, sizeof line - n, " %02x", c);
        while (n < sizeof line - 4 && next(&c, 50000));
        say(log, line);
    }
    SetMode(Input(), 0);
    fclose(log);
    return RETURN_OK;
}
