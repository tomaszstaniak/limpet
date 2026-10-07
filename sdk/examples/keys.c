/* limpet-keys: read single key presses in raw mode and name them.

   The keyboard part uses only dos.library (SetMode, WaitForChar, Read), so
   it behaves the same in Limpet and in CON:; the SDK is used only to decide
   whether colour may be added. The two consoles differ in one visible way:
   CON: sends cursor keys as the 8-bit CSI 0x9b, Limpet as ESC [. Both are
   decoded here.

   Raw mode is always undone: on q, on Ctrl-C (byte 3 in raw mode) and after
   60 seconds without a key. */
#include <limpet/limpet.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

static int next(unsigned char *c, LONG microseconds)
{
    return WaitForChar(Input(), microseconds) && Read(Input(), c, 1) == 1;
}

int main(void)
{
    LimpetPresentation p;
    char cyan[LIMPET_SEQUENCE_MAX] = "";
    const char *off = "";
    unsigned char c;

    if (!IsInteractive(Input())) {
        puts("Input is not a console.");
        return RETURN_WARN;
    }
    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK &&
        limpet_sgr_ansi(cyan, sizeof cyan, &p, 6, 0))
        off = LIMPET_SGR_FG_DEFAULT;
    puts("Press keys; q or Ctrl-C ends, as does 60 s without a key.");
    fflush(stdout);

    if (!SetMode(Input(), 1)) {
        puts("The console cannot switch to raw mode.");
        return RETURN_WARN;
    }
    for (;;) {
        const char *name = NULL;
        char text[16];
        if (!next(&c, 60000000)) { printf("timeout\r\n"); break; }
        if (c == 'q' || c == 3) { printf("%s\r\n", c == 3 ? "Ctrl-C" : "q"); break; }
        if (c == 0x1b || c == 0x9b) {
            unsigned char d;
            /* A lone Esc has nothing after it within 50 ms. */
            if (c == 0x1b && (!next(&d, 50000) || d != '[')) name = "Esc";
            else if (next(&d, 50000)) {
                name = d == 'A' ? "Up" : d == 'B' ? "Down" : d == 'C' ? "Right" :
                       d == 'D' ? "Left" : "other key sequence";
                /* Drop the rest of a longer sequence (function keys end
                   in '~'): read up to its final byte, 0x40..0x7e. */
                while (!(d >= 0x40 && d <= 0x7e) && next(&d, 20000)) {}
            } else name = "incomplete sequence";
            printf("%s%-8s%s %s\r\n", cyan, name, off,
                   !strcmp(name, "Esc") ? "" :
                   c == 0x9b ? "(8-bit CSI, as CON: sends it)" : "(ESC [)");
        } else if (c < 0x20 || c == 0x7f) {
            printf("control 0x%02x\r\n", c);
        } else {
            snprintf(text, sizeof text, "'%c'", c >= 0x80 ? '?' : c);
            printf("%s%-8s%s byte 0x%02x\r\n", cyan, text, off, c);
        }
        fflush(stdout);
    }
    fflush(stdout);
    if (!SetMode(Input(), 0)) {
        puts("The console did not return to cooked mode.");
        return RETURN_WARN;
    }
    return RETURN_OK;
}
