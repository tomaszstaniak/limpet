/* limpet-progress: a progress bar that rewrites one line in Limpet and
   prints a line per 10% everywhere else.

   The bar width follows the window: the size is a snapshot, so it is
   asked again before each redraw, at most twice a second. Ctrl-C stops
   the run, ends the line and returns WARN like any AROS command. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#define STEPS 100

static int broken(void)
{
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

int main(void)
{
    LimpetPresentation p;
    char green[LIMPET_SEQUENCE_MAX], bar[256];
    int rich, step;
    unsigned columns = 0;

    fflush(stdout);
    rich = limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK &&
           limpet_can_rewrite_line(&p);
    if (rich) limpet_sgr_ansi(green, sizeof green, &p, 2, 0);
    puts("Copying 100 blocks (simulated).");

    for (step = 0; step <= STEPS; ++step) {
        if (broken()) {
            /* Leave the cursor on a fresh line with default colours. */
            printf("%s\n***Break at %d%%\n", rich ? LIMPET_SGR_FG_DEFAULT : "", step);
            return RETURN_WARN;
        }
        if (rich) {
            unsigned width, filled, i;
            if (step % 25 == 0) {
                /* A resize between two queries shows up at the next one. */
                fflush(stdout);
                if (limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK)
                    columns = limpet_columns(&p);
            }
            /* "[" bar "] 100%" plus one spare column: writing into the last
               column would wrap in some terminals. */
            width = columns > 12 ? columns - 9 : 10;
            if (width > sizeof bar - 1) width = sizeof bar - 1;
            filled = width * (unsigned)step / STEPS;
            for (i = 0; i < width; ++i) bar[i] = i < filled ? '#' : '.';
            bar[width] = 0;
            printf("\r[%s%.*s%s%s] %3d%%%s", green, (int)filled, bar, LIMPET_SGR_FG_DEFAULT,
                   bar + filled, step, LIMPET_ERASE_EOL);
            fflush(stdout);
        } else if (step % 10 == 0) {
            printf("%d%%\n", step);
        }
        Delay(5);   /* 0.1 s per block */
    }
    if (rich) putchar('\n');
    puts("Done.");
    return RETURN_OK;
}
