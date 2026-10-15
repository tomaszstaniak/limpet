#ifndef XTERM_SETTINGS_H
#define XTERM_SETTINGS_H
#include <stddef.h>
/* Limpet's settings file: ENV:Limpet/Limpet.conf, else ENVARC:Limpet/Limpet.conf
 * (read by the frontend; this model only parses). One key=value per line,
 * # comments and blank lines; spaces around the value are dropped, so a path
 * may hold spaces inside. A file is taken whole or not at all: any unknown or
 * repeated key, bad number, value out of range or line without '=' rejects
 * it, so a half-understood file never changes what Limpet does. */
#define XSETTINGS_FILE_MAX 4096
#define XSETTINGS_PATH_MAX 256

/* Scrollback: lines kept at the widest window (160 columns); a narrower
 * window keeps more of its shorter lines. Each line costs about 27 KB (the
 * text and the second buffer that reflow needs). */
#define XSETTINGS_SCROLLBACK_DEFAULT 256
#define XSETTINGS_SCROLLBACK_MIN 100
#define XSETTINGS_SCROLLBACK_MAX 1000
/* Commands kept for Up/Down and Ctrl-R, 1 KB each. */
#define XSETTINGS_HISTORY_DEFAULT 32
#define XSETTINGS_HISTORY_MIN 1
#define XSETTINGS_HISTORY_MAX 500
/* Pixel size of the main font; the cell must stay within 32x32 pixels. */
#define XSETTINGS_FONT_SIZE_DEFAULT 13
#define XSETTINGS_FONT_SIZE_MIN 8
#define XSETTINGS_FONT_SIZE_MAX 20

enum { XSET_FONT=1, XSET_FONT_SIZE=2, XSET_THEME=4, XSET_SCROLLBACK=8, XSET_HISTORY=16 };
typedef struct {
    char font[XSETTINGS_PATH_MAX];   /* empty: the built-in Vera Mono */
    char theme[XSETTINGS_PATH_MAX];  /* empty: the built-in dark theme */
    unsigned font_size, scrollback, history;
    unsigned given;                  /* XSET_* the file set */
} XSettings;

void settings_default(XSettings *);
/* 0: the file's values are in *s. Otherwise the 1-based number of the first
 * line that is wrong (or 1 for a file that is too large or not text), *s is
 * unchanged and *why says what is wrong. */
int settings_parse(XSettings *s, const char *text, size_t length, const char **why);
#endif
