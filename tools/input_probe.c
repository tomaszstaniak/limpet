/* input-probe: record exactly what a raw-mode client receives.

     input-probe FILE [modes] [seconds] [keep]

   Switches Input() to raw mode, turns on the DEC private modes given as a
   comma list (for example 2004,2048,2100; "none" turns on nothing), and
   writes every input byte to FILE, escaped, one line per read, with the
   elapsed time. Ends on the byte 'Q' or after the given seconds (default
   60). Without "keep" it turns the modes off again before cooked mode; with
   "keep" it leaves them on, to test that the terminal itself clears them.
   A test tool for Limpet's input ordering, not an example. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long now_ms(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (d.ds_Days * 1440L + d.ds_Minute) * 60000L + d.ds_Tick * (1000L / TICKS_PER_SECOND);
}

static void modes(const char *list, char on)
{
    char copy[64], *m;
    if (!strcmp(list, "none")) return;
    snprintf(copy, sizeof copy, "%s", list);
    for (m = strtok(copy, ","); m; m = strtok(NULL, ",")) printf("\033[?%s%c", m, on);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *list = argc > 2 ? argv[2] : "2004,2048";
    long seconds = argc > 3 ? atol(argv[3]) : 60, start, deadline;
    int keep = argc > 4 && !strcmp(argv[4], "keep"), done = 0;
    FILE *log;
    if (argc < 2 || !(log = fopen(argv[1], "w"))) { puts("usage: input-probe FILE [modes] [seconds] [keep]"); return RETURN_ERROR; }
    if (!SetMode(Input(), 1)) { fputs("raw mode refused\n", log); fclose(log); return RETURN_WARN; }
    modes(list, 'h');
    printf("input-probe: modes %s, %ld s, Q ends\r\n", list, seconds);
    fflush(stdout);
    start = now_ms(); deadline = start + seconds * 1000;
    while (!done && now_ms() < deadline) {
        unsigned char buf[256];
        LONG n, i;
        if (!WaitForChar(Input(), 100000)) continue;
        n = Read(Input(), buf, sizeof buf);
        if (n <= 0) { fputs("read failed\n", log); break; }
        fprintf(log, "%6ld ", now_ms() - start);
        for (i = 0; i < n; ++i) {
            unsigned c = buf[i];
            if (c == 'Q') done = 1;
            if (c == 0x1b) fputs("<ESC>", log);
            else if (c == '\r') fputs("<CR>", log);
            else if (c == '\n') fputs("<LF>", log);
            else if (c < 0x20 || c == 0x7f || c >= 0x80) fprintf(log, "<%02x>", c);
            else fputc((int)c, log);
        }
        fputc('\n', log);
        fflush(log);
    }
    if (!keep) modes(list, 'l');
    fputs(done ? "end: Q\n" : "end: time\n", log);
    fclose(log);
    SetMode(Input(), 0);
    puts(keep ? "input-probe: modes left on" : "input-probe: modes off");
    return RETURN_OK;
}
