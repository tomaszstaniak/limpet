/* Guest probe for Ctrl-C in a console session.

   break_probe wait SECONDS
       Polls SIGBREAKF_CTRL_C every 60 ms (3 ticks) and says whether it arrived:
       "break_probe: Ctrl-C after N ms" (return 5) or "... no break" (0).
   break_probe raw SECONDS
       Puts Input() into raw mode, waits up to SECONDS for one byte and
       prints it in hex, then restores cooked mode. In raw mode Ctrl-C must
       arrive as byte 03 and must not raise SIGBREAKF_CTRL_C.
   Plain DOS calls only, so it runs the same under CON: and Limpet. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    long seconds, ms;
    if (argc != 3 || (strcmp(argv[1], "wait") && strcmp(argv[1], "raw"))) {
        printf("usage: break_probe wait|raw SECONDS\n");
        return 10;
    }
    seconds = strtol(argv[2], NULL, 10);
    if (seconds < 1 || seconds > 600) seconds = 30;
    SetSignal(0, SIGBREAKF_CTRL_C);
    if (!strcmp(argv[1], "wait")) {
        for (ms = 0; ms < seconds * 1000; ms += 60) {
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
                printf("break_probe: Ctrl-C after about %ld ms\n", ms);
                return 5;
            }
            Delay(3);
        }
        printf("break_probe: no break in %ld s\n", seconds);
        return 0;
    }
    {
        BPTR in = Input();
        unsigned char byte = 0;
        LONG got = 0, broke;
        if (!SetMode(in, 1)) { printf("break_probe: SetMode raw failed\n"); return 10; }
        printf("break_probe: raw, press a key\n");
        fflush(stdout);
        if (WaitForChar(in, seconds * 1000000L)) got = Read(in, &byte, 1);
        broke = (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
        SetMode(in, 0);
        if (got == 1) printf("break_probe: raw byte %02x, signal %s\n", byte, broke ? "raised" : "not raised");
        else printf("break_probe: raw, no byte in %ld s, signal %s\n", seconds, broke ? "raised" : "not raised");
        return 0;
    }
}
