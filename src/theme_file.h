#ifndef XTERM_THEME_FILE_H
#define XTERM_THEME_FILE_H
/* Colour files of other terminals, read in place: Ghostty and Kitty theme
 * files (line formats) and iTerm2 colour presets (theme_iterm.h), beside
 * Limpet's own format (theme.h). The format is recognised by content, not
 * by the file's name. Only colours are taken: nothing in a file is
 * included, expanded or run, and settings that are not colours are skipped
 * and counted. */
#include "theme.h"
#include <stddef.h>
#define XTHEME_FILE_MAX 65536
enum { XTHEME_FORMAT_NATIVE, XTHEME_FORMAT_LINES, XTHEME_FORMAT_ITERM };
enum { XTHEME_OK, XTHEME_CANNOT_READ, XTHEME_TOO_LARGE, XTHEME_NOT_TEXT,
       XTHEME_BINARY_PLIST, XTHEME_NOT_A_THEME_FORMAT, XTHEME_NO_COLOURS,
       XTHEME_BAD_COLOUR, XTHEME_BAD_NATIVE };
typedef struct {
    int error;          /* XTHEME_* */
    int line;           /* 1-based, for XTHEME_BAD_COLOUR; else 0 */
    int format;         /* XTHEME_FORMAT_* of a file that loaded */
    unsigned ignored;   /* settings in the file that are not colours Limpet uses */
} XThemeReport;
/* The whole file in memory. 0: *t is the file's theme, complete
 * (theme_complete), with t->bold_bright kept. -1: *t is unchanged and
 * r->error says why. A file in Limpet's own format is read by theme_parse
 * over the built-in dark theme, as before. */
int theme_file_parse(XTheme *t, const char *bytes, size_t length, XThemeReport *r);
int theme_file_load(XTheme *t, const char *path, XThemeReport *r);
/* The title bar's sentence for a report. instead says what happens then:
 * "built-in dark theme" or "nothing changed". Empty for a load with nothing
 * to say. name is how the user named the theme. */
void theme_file_message(char *out, size_t size, const char *name, const XThemeReport *r, const char *instead);
/* A theme= value as a path. A value with ':' or '/' is a path already. A
 * bare name is looked for in the user's drawer and then in the program's,
 * as given and then with .theme, .conf and .itermcolors. 0 and the path in
 * out, or -1 when nothing by that name exists (or it does not fit). */
#define XTHEME_USER_DRAWER "ENVARC:Limpet/Themes"
#define XTHEME_PROGRAM_DRAWER "PROGDIR:Themes"
int theme_locate(char *out, size_t size, const char *value, int (*exists)(const char *path));
#endif
