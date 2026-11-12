#ifndef XTERM_CONSOLE_NAME_H
#define XTERM_CONSOLE_NAME_H
/* The name a program opens a console by, as CON: reads it
 * (rom/filesys/console_handler/support.c parse_filename, ABIv11 e33ca77d and
 * mainline alike): DEVICE:left/top/width/height/title/option/option...
 *
 * - Everything up to the first ':' is the device and is skipped.
 * - Fields 1-4 are decimal pixels, an empty field keeps the default; any
 *   other character there makes the name bad (ERROR_BAD_STREAM_NAME).
 * - Field 5 is the title, verbatim (it cannot hold '/').
 * - Later fields are options, compared without case against the keywords
 *   in CON:'s order, and a field matches a keyword it is a prefix of, so
 *   "C" is CLOSE and "S" is SIMPLE, as in CON:. SCREEN and WINDOW take the
 *   rest of the field as their value. Unknown options are ignored. */
#define XCON_TITLE_MAX 128
#define XCON_SCREEN_MAX 64
enum {
    XCON_WAIT = 1 << 0, XCON_CLOSE = 1 << 1, XCON_NOCLOSE = 1 << 2, XCON_AUTO = 1 << 3,
    XCON_INACTIVE = 1 << 4, XCON_NODEPTH = 1 << 5, XCON_NOSIZE = 1 << 6, XCON_NODRAG = 1 << 7,
    XCON_NOBORDER = 1 << 8, XCON_BACKDROP = 1 << 9, XCON_SIMPLE = 1 << 10, XCON_SMART = 1 << 11,
    XCON_ALT = 1 << 12, XCON_WINDOW = 1 << 13, XCON_SCREEN = 1 << 14, XCON_BOOT = 1 << 15
};
typedef struct {
    long left, top, width, height;   /* -1: not given */
    char title[XCON_TITLE_MAX];      /* empty: not given */
    char screen[XCON_SCREEN_MAX];    /* SCREEN's public screen name */
    unsigned long window;            /* WINDOW's value */
    unsigned options;                /* XCON_*, in the order given: a later
                                        CLOSE or NOCLOSE wins over an earlier */
    int close_gadget;                /* -1 not said, 0 NOCLOSE, 1 CLOSE */
} XConsoleName;
/* 0, or -1 for a bad name (a non-digit in fields 1-4, a title or screen name
 * too long). name may be the whole "XTERM:..." or what follows the colon. */
int console_name_parse(XConsoleName *, const char *name);
/* Whether the name carries WINDOW (fields from the sixth on, six letters
 * without case and then the value), by the same rule as the parser. For
 * the handler's launcher, which has no C library: nothing is called. */
static inline int console_name_has_window(const char *name)
{
    const char *p = name, *field;
    unsigned id = 1;
    while (*p && *p != ':') ++p;
    field = *p ? p + 1 : name;
    for (;;) {
        const char *end = field;
        while (*end && *end != '/') ++end;
        if (id > 5 && end - field >= 6) {
            static const char word[] = "window";
            unsigned i;
            for (i = 0; i < 6; ++i) {
                int c = (unsigned char)field[i];
                if (c >= 'A' && c <= 'Z') c += 32;
                if (c != word[i]) break;
            }
            if (i == 6) return 1;
        }
        if (!*end) return 0;
        field = end + 1; ++id;
    }
}
#endif
