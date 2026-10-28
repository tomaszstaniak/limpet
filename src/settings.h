#ifndef XTERM_SETTINGS_H
#define XTERM_SETTINGS_H
#include <stddef.h>
/* Limpet's settings files: ENV:Limpet/Limpet.conf, else ENVARC:Limpet/Limpet.conf,
 * and Session.conf beside it (read by the frontend; this model only parses). One key=value per line,
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

/* What a new Shell window runs after S:Shell-Startup (session_start.h):
 * nothing, Fastfetch in one of its looks, or the user's own command. */
#define XSETTINGS_COMMAND_MAX 400
enum { XSTART_OFF, XSTART_FASTFETCH, XSTART_COMMAND };
enum { XFF_COMPACT, XFF_PICTURE, XFF_FILE };          /* fastfetch_look */
enum { XFF_LOGO_LOOK, XFF_LOGO_TEXT, XFF_LOGO_NONE };  /* fastfetch_logo */
/* fastfetch_hide: groups of lines left out, as the packaged presets name
 * them (system, hardware, memory-disks, network). */
enum { XFF_SYSTEM=1, XFF_HARDWARE=2, XFF_MEMORY=4, XFF_NETWORK=8 };
#define XFF_GROUPS 4
extern const char *const settings_group_names[XFF_GROUPS];

enum { XSET_FONT=1, XSET_FONT_SIZE=2, XSET_THEME=4, XSET_SCROLLBACK=8, XSET_HISTORY=16,
       XSET_SESSION_START=32, XSET_SESSION_COMMAND=64, XSET_FASTFETCH=128,
       XSET_FASTFETCH_LOOK=256, XSET_FASTFETCH_CONFIG=512, XSET_FASTFETCH_HIDE=1024,
       XSET_FASTFETCH_LOGO=2048 };
typedef struct {
    char font[XSETTINGS_PATH_MAX];   /* empty: the built-in Vera Mono */
    char theme[XSETTINGS_PATH_MAX];  /* empty: the built-in dark theme */
    unsigned font_size, scrollback, history;
    unsigned session_start;          /* XSTART_* */
    char session_command[XSETTINGS_COMMAND_MAX]; /* a Shell command line */
    char fastfetch[XSETTINGS_PATH_MAX];          /* empty: "fastfetch" by the Path */
    unsigned fastfetch_look;         /* XFF_COMPACT.. */
    char fastfetch_config[XSETTINGS_PATH_MAX];   /* for XFF_FILE */
    unsigned fastfetch_hide;         /* XFF_SYSTEM.. bits */
    unsigned fastfetch_logo;         /* XFF_LOGO_* */
    unsigned given;                  /* XSET_* the file set */
} XSettings;

void settings_default(XSettings *);
/* 0: the file's values are in *s. Otherwise the 1-based number of the first
 * line that is wrong (or 1 for a file that is too large or not text), *s is
 * unchanged and *why says what is wrong. Limpet.conf takes the font, theme,
 * scrollback and history keys, as in 0.7.1 and before. */
int settings_parse(XSettings *s, const char *text, size_t length, const char **why);
/* The same for Session.conf (ENV:Limpet/, else ENVARC:Limpet/), which holds
 * only the session-start keys: a file of its own, so that a Limpet older
 * than 0.8.0, which refuses a Limpet.conf with keys it does not know, never
 * sees them. Written by Limpet Preferences (settings_edit.h). */
int settings_parse_session(XSettings *s, const char *text, size_t length, const char **why);
#endif
