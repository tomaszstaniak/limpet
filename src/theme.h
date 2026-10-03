#ifndef XTERM_THEME_H
#define XTERM_THEME_H
#include "screen_model.h"
typedef struct XTheme {
    uint32_t ansi[16];
    uint32_t foreground, background, cursor, selection_fg, selection_bg;
} XTheme;
void theme_default(XTheme *);
/* Partial overrides of the supplied theme; errors leave it unchanged.
 * Strict key=#RRGGBB format, one key per line; # comments and blank lines.
 * File limit 4096 bytes. No allocation, code execution or recursive includes. */
int theme_parse(XTheme *, const char *);
int theme_load(XTheme *, const char *);
uint32_t theme_fg(const XTheme *, const XTermCell *);
uint32_t theme_bg(const XTheme *, const XTermCell *);
#endif
