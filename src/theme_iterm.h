#ifndef XTERM_THEME_ITERM_H
#define XTERM_THEME_ITERM_H
/* iTerm2 colour presets (.itermcolors): an XML property list whose outer
 * dictionary maps "Ansi 0 Color", "Background Color" and so on to
 * dictionaries of colour components, fractions 0 to 1. Read with a scanner
 * for this one shape, not an XML parser. The plain keys are taken; the
 * "(Light)" and "(Dark)" sets of newer files are skipped. Colours in the
 * Display P3 space are converted to sRGB and clamped. */
#include "theme_file.h"
/* Sets the roles the file gives in *t and their bits in *seen (theme.h,
 * XROLE_*); the caller completes the theme. Always 0: a file without usable
 * colours simply sets nothing. r->ignored counts colours Limpet has no use
 * for ("Bold Color"). */
int theme_iterm_parse(XTheme *t, unsigned long *seen, const char *bytes, size_t length, XThemeReport *r);
#endif
