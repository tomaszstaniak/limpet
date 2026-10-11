/* limpet-hello: ask the console what it renders and print the answer.

   Works in any console. In Limpet it prints what the terminal reported; in
   CON:, in another console or with output redirected to a file it says so
   and prints plain text only. It never reads the keyboard. */
#include <limpet/limpet.h>
#include <proto/dos.h>
#include <stdio.h>

static const char *level_name(uint32_t level)
{
    switch (level) {
    case XTY_COLOR_ANSI16: return "16 colours (SGR 30-37, 90-97)";
    case XTY_COLOR_INDEXED256: return "256 colours (SGR 38;5;n)";
    case XTY_COLOR_RGB: return "24-bit colour (SGR 38;2;r;g;b)";
    default: return "none";
    }
}

static const char *yes_no(int value) { return value ? "yes" : "no"; }

int main(void)
{
    LimpetPresentation p;
    LONG error;
    LimpetStatus status;

    /* Text already in stdio's buffer must reach the console before the
       packet does; here there is none yet, but the habit matters. */
    fflush(stdout);
    status = limpet_query_presentation(Output(), &p, &error);
    printf("Limpet SDK %s\n", LIMPET_SDK_VERSION_STRING);
    switch (status) {
    case LIMPET_OK:
        break;
    case LIMPET_NOT_INTERACTIVE:
        puts("Output is not a console (a file or a pipe): plain text only.");
        return RETURN_OK;
    case LIMPET_UNSUPPORTED:
        puts("This console does not answer the Limpet query (CON: or another console).");
        puts("Nothing is known about it: plain text only.");
        return RETURN_OK;
    case LIMPET_INVALID_REPLY:
        puts("The console sent a malformed reply: plain text only.");
        return RETURN_WARN;
    default:
        printf("The query failed, DOS error %ld: plain text only.\n", (long)error);
        return RETURN_WARN;
    }

    puts("Output is a Limpet console.");
    if (p.known & XTY_PRESENT_KNOWN_COLORS)
        printf("  Colours:       %s\n", level_name(limpet_color_level(&p)));
    else
        puts("  Colours:       unknown");
    if (p.known & XTY_PRESENT_KNOWN_INTERPRETATION)
        printf("  SGR 30-37:     %s\n", limpet_uses_pens(&p) ?
               "Workbench screen pens; ANSI colours go as 38;5;0-15" : "ANSI colours");
    else
        puts("  SGR 30-37:     unknown, assume ANSI colours");
    if (p.known & XTY_PRESENT_KNOWN_STYLES)
        printf("  Bold, reset:   %s\n", yes_no(limpet_has_bold(&p)));
    if (p.known & XTY_PRESENT_KNOWN_LINES)
        printf("  Rewrite line:  %s (CR and CSI K)\n", yes_no(limpet_can_rewrite_line(&p)));
    if (limpet_columns(&p))
        printf("  Size:          %lu columns x %lu rows, when asked\n",
               (unsigned long)limpet_columns(&p), (unsigned long)limpet_rows(&p));
    else
        puts("  Size:          unknown");
    /* Input modes a raw-mode program may turn on. Limpet 0.1.0 does not
       report this field, so "unknown" is the honest answer there. */
    if (p.known & XTY_PRESENT_KNOWN_INPUT)
        printf("  Input modes:   %s%s%s%s\n",
               p.input & XTY_INPUT_BRACKETED_PASTE ? "bracketed paste (2004) " : "",
               p.input & XTY_INPUT_RESIZE_REPORT ? "resize reports (2048) " : "",
               p.input & XTY_INPUT_WHEEL_REPORT ? "wheel reports (2100)" : "",
               p.input ? "" : "none");
    else
        puts("  Input modes:   unknown (Limpet 0.1.0 does not report them)");
    /* Window title by OSC 0/2: confirmed by Limpet 0.3.0 and later only. */
    if (p.known & XTY_PRESENT_KNOWN_FEATURES)
        printf("  Window title:  %s\n", limpet_can_set_title(&p) ? "settable (OSC 0/2)" : "no");
    else
        puts("  Window title:  unknown (Limpet before 0.3.0 does not report it)");
    if (p.known & ~(uint32_t)(XTY_PRESENT_KNOWN_COLORS | XTY_PRESENT_KNOWN_STYLES |
                              XTY_PRESENT_KNOWN_LINES | XTY_PRESENT_KNOWN_GEOMETRY |
                              XTY_PRESENT_KNOWN_INTERPRETATION | XTY_PRESENT_KNOWN_INPUT |
                              XTY_PRESENT_KNOWN_FEATURES))
        puts("  The terminal also reported fields newer than this SDK; ignored.");
    return RETURN_OK;
}
