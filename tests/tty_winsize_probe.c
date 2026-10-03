/* Standard-libc terminal state probe: geometry from TIOCGWINSZ and the
   canonical/echo flags, printed and optionally appended to a result file.
   Run after a raw-mode application exits to check that the terminal it
   leaves behind has the window's geometry and cooked input again. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct winsize ws;
    struct termios t;
    char line[160];
    int size_ok, attr_ok, size_errno, attr_errno;

    memset(&ws, 0, sizeof ws);
    size_ok = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0;
    size_errno = size_ok ? 0 : errno;
    attr_ok = tcgetattr(STDIN_FILENO, &t) == 0;
    attr_errno = attr_ok ? 0 : errno;
    snprintf(line, sizeof line,
             "WINSIZE %s rows=%u cols=%u errno=%d TERMIOS %s icanon=%d echo=%d errno=%d\n",
             size_ok ? "ok" : "fail", (unsigned)ws.ws_row, (unsigned)ws.ws_col, size_errno,
             attr_ok ? "ok" : "fail", attr_ok ? !!(t.c_lflag & ICANON) : -1,
             attr_ok ? !!(t.c_lflag & ECHO) : -1, attr_errno);
    fputs(line, stdout);
    if (argc == 2) {
        FILE *f = fopen(argv[1], "a");
        if (!f || fputs(line, f) < 0 || fclose(f) != 0)
            return 10;
    }
    return size_ok && attr_ok ? 0 : 5;
}
