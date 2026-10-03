/* Visual DECSTBM and region-operations smoke test for the DOS/Shell frontend.
 * The fixed layout intentionally fits inside the default 80x24 window. */
#include <stdio.h>
#include <proto/dos.h>

int main(void)
{
    unsigned line;
    fputs("\033[?1049h\033[?25l\033[2J\033[2;10r", stdout);
    fputs("\033[1;1HAROS Term / scrolling-region GUI smoke test", stdout);
    fputs("\033[11;1H\033[7m STATUS: PINNED BELOW LOG                       \033[27m", stdout);
    fputs("\033[12;1H\033[7m DECSTBM 2..10 / 40 LOG LINES / NO FULL SCROLL    \033[27m", stdout);
    fputs("\033[2;1H", stdout);
    for (line = 1; line <= 40; ++line) {
        printf("Log line %02u / 40 - region should move; header and footer should not", line);
        if (line != 40) fputs("\r\n", stdout);
        fflush(stdout);
        Delay(2);
    }
    Delay(75);

    /* After S(2), T(1), and reverse index at the top margin, original
     * ROW 04 must be at row 4 and ROW 10 at row 10. The pinned rows remain. */
    fputs("\033[r\033[H\033[2J\033[2;10r", stdout);
    fputs("\033[1;1HAROS Term / CSI S/T + ESC M GUI smoke test", stdout);
    fputs("\033[11;1H\033[7m PINNED FOOTER / REGION 2..10                 \033[27m", stdout);
    fputs("\033[12;1HEXPECT: blank rows 2-3, ROW 04..ROW 10 on rows 4..10", stdout);
    for (line = 2; line <= 10; ++line)
        printf("\033[%u;1HROW %02u / initial position %02u", line, line, line);
    fputs("\033[2S\033[1T\033[2;1H\033M", stdout);
    fflush(stdout);
    Delay(1000); /* Leave the region-operations frame visible. */
    fputs("\033[r\033[?25h\033[?1049l", stdout);
    fflush(stdout);
    return 0;
}
