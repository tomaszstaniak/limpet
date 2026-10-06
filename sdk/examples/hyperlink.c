/* limpet-hyperlink: OSC 8 links with a text fallback.

   In Limpet a link shows its label; hovering shows the target in the title
   bar, Ctrl+click opens it through OpenURL, Ctrl+Shift+click copies it.
   Anywhere else the target is printed after the label, so nothing is lost.

   The OSC 8 sequence is sent only to a console that answered the query:
   CON: would print it as text. */
#include <limpet/limpet.h>
#include <proto/dos.h>
#include <stdio.h>

static int rich;

static void link(const char *label, const char *uri)
{
    char open[LIMPET_OSC8_URI_MAX + 8];
    /* limpet_osc8_open refuses a URI with spaces, controls or non-ASCII
       bytes; the label is then printed with the URI as plain text. */
    if (rich && limpet_osc8_open(open, sizeof open, uri))
        printf("%s%s%s", open, label, LIMPET_OSC8_CLOSE);
    else
        printf("%s <%s>", label, uri);
}

int main(void)
{
    LimpetPresentation p;

    fflush(stdout);
    rich = limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK;
    fputs("Project page: ", stdout);
    link("Limpet on GitHub", "https://github.com/tomaszstaniak/limpet");
    fputs("\nAROS:         ", stdout);
    link("aros.org", "https://www.aros.org/");
    fputs("\nNot a valid URI (has a space), so shown as text: ", stdout);
    link("example", "https://example.org/a b");
    putchar('\n');
    if (rich)
        puts("Ctrl+click a label to open it (needs OpenURL), Ctrl+Shift+click to copy it.");
    return RETURN_OK;
}
