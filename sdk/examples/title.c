/* limpet-title: say in the window title what the program is doing.

   A program that runs for a while (a build, a download, an assistant
   waiting for a model) can name its state in Limpet's title bar, so a user
   who looks at another window still sees it. The title is sent only when
   the state changes, never with each line of output. Limpet drops the
   title by itself when the program ends; LIMPET_TITLE_RESET is for a
   program that wants its plain name back while it still runs.

   Only when the terminal confirmed it (Limpet 0.3.0 and later): an older
   Limpet answers the presentation query without confirming titles, and
   CON: would print the sequence as text. Ctrl-C stops early. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

static const char *const states[] = {
    "title demo: connecting",
    "title demo: waiting for the server",
    "title demo: receiving",
    "title demo: done",
};

int main(void)
{
    LimpetPresentation p;
    char sequence[LIMPET_TITLE_MAX + 8];
    unsigned i;
    int titles;

    fflush(stdout);
    titles = limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK &&
             limpet_can_set_title(&p);
    if (!titles) puts("This terminal does not confirm window titles; states follow as text.");
    for (i = 0; i < sizeof states / sizeof *states; ++i) {
        size_t n;
        printf("%s\n", states[i]);
        fflush(stdout);
        if (titles && (n = limpet_osc_title(sequence, sizeof sequence, &p, states[i])))
            limpet_write_dos((void *)Output(), sequence, n);
        /* Two seconds per state, checked every 0.1 s so Ctrl-C answers at once. */
        {
            int t;
            for (t = 0; t < 100; t += 5) {
                if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
                    puts("***Break");
                    return RETURN_WARN;
                }
                Delay(5);
            }
        }
    }
    return RETURN_OK;
}
