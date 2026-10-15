/* Guest regression for the TIOCGWINSZ crash of Limpet 0.4.1 and earlier:
 * the C library's ioctl reads the console's size through ACTION_DISK_INFO's
 * id_InUse, which Limpet left empty, and died in crt.library. Run it in a
 * Limpet window (and after resizing it):
 *     winsize-check [LOG]
 * Return 0 when the size is that of a Limpet grid (1..80 rows,
 * 1..160 columns), 5 otherwise; a crash is the failure it exists for. */
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct winsize ws;
    char line[96];
    int ok;
    memset(&ws, 0, sizeof ws);
    ok = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 &&
         ws.ws_row >= 1 && ws.ws_row <= 80 && ws.ws_col >= 1 && ws.ws_col <= 160;
    snprintf(line, sizeof line, "%s winsize rows=%u cols=%u\n", ok ? "PASS" : "FAIL",
             (unsigned)ws.ws_row, (unsigned)ws.ws_col);
    fputs(line, stdout);
    if (argc == 2) {
        FILE *f = fopen(argv[1], "a");
        if (f) { fputs(line, f); fclose(f); }
    }
    return ok ? 0 : 5;
}
