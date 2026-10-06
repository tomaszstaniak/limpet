/* Writes the presentation reply the terminal makes now, as raw bytes, for
 * test_sdk_compat.sh to decode with an older SDK. */
#include "../../src/xterm_presentation.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    XtyControl c;
    FILE *f;
    if (argc != 2) return 2;
    memset(&c, 0, sizeof c);
    c.version = XTY_CONTROL_VERSION; c.operation = XTY_GET_PRESENTATION;
    if (!xterm_presentation_reply(&c, sizeof c, 24, 80, XTY_COLOR_PENS)) return 1;
    f = fopen(argv[1], "wb");
    if (!f || fwrite(&c, sizeof c, 1, f) != 1) return 1;
    return fclose(f) ? 1 : 0;
}
