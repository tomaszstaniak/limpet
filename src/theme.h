#ifndef XTERM_THEME_H
#define XTERM_THEME_H
#include "screen_model.h"
typedef struct XTheme {
    uint32_t ansi[16];
    uint32_t foreground, background, cursor, selection_fg, selection_bg;
    /* Workbench pens 2..7 of the theme's own (native key pen2..pen7), used
       as given; a pen not given comes from the palette (theme_pen). */
    uint32_t pen[8];
    unsigned pen_set;          /* bit n: pen[n] was given */
    /* Bold text in palette colours 0..7 is shown in 8..15. A setting, not
       part of a theme file; on in theme_default. */
    unsigned char bold_bright;
} XTheme;
void theme_default(XTheme *);
/* Partial overrides of the supplied theme; errors leave it unchanged.
 * Strict key=#RRGGBB format, one key per line; # comments and blank lines.
 * Keys: foreground, background, cursor, selection_fg, selection_bg,
 * ansi0..ansi15, pen2..pen7.
 * File limit 4096 bytes. No allocation, code execution or recursive includes. */
int theme_parse(XTheme *, const char *);
int theme_load(XTheme *, const char *);
/* What a file gave, for theme_complete: bit n (0..15) is palette entry n. */
enum { XROLE_FG=1<<16, XROLE_BG=1<<17, XROLE_CURSOR=1<<18, XROLE_SEL_FG=1<<19, XROLE_SEL_BG=1<<20 };
/* Fills every role the file did not give: the bright half from the normal
 * one, the grounds from palette 7 and 0, the cursor from the foreground,
 * the selection as inverse video, and a palette to suit a light or dark
 * background when the file has none. -1, with *t as it was, when the file
 * gave neither both grounds nor eight palette entries: not a theme. */
int theme_complete(XTheme *, unsigned long seen);
/* The colour of Workbench pen 0..7 (the AROS Shell's prompt uses pens).
 * Pens the theme does not give come from the palette, kept legible: the
 * accent (pen 3) is never the text colour or close to pen 2, and pens 4..7
 * too close to the background are the text colour. */
uint32_t theme_pen(const XTheme *, unsigned pen);
/* Brightness 0..255, weights 299/587/114. */
unsigned theme_luma(uint32_t rgb);
uint32_t theme_fg(const XTheme *, const XTermCell *);
uint32_t theme_bg(const XTheme *, const XTermCell *);
#endif
