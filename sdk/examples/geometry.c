/* limpet-geometry: print the window size, then keep asking for 20 seconds
   and print each change. Resize the window while it runs.

   This is how size works in Limpet 0.1: each query returns the size at that
   moment. There is no resize notification, so a program that lays out text
   asks again before it draws. Ends by itself, or with Ctrl-C. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

int main(void)
{
    LimpetPresentation p;
    uint32_t rows = 0, columns = 0;
    int tick;

    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK || !limpet_columns(&p)) {
        puts("This console does not report its size. Assume 80 columns, or ask the user.");
        return RETURN_OK;
    }
    puts("Resize the window; changes are printed for 20 seconds (Ctrl-C stops).");
    for (tick = 0; tick < 40; ++tick) {
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            puts("***Break");
            return RETURN_WARN;
        }
        fflush(stdout);
        if (limpet_query_presentation(Output(), &p, NULL) != LIMPET_OK) {
            puts("The console stopped answering.");
            return RETURN_WARN;
        }
        if (limpet_rows(&p) != rows || limpet_columns(&p) != columns) {
            rows = limpet_rows(&p);
            columns = limpet_columns(&p);
            printf("%3d.%d s  %lu columns x %lu rows\n", tick / 2, tick % 2 * 5,
                   (unsigned long)columns, (unsigned long)rows);
        }
        Delay(25);   /* half a second */
    }
    return RETURN_OK;
}
