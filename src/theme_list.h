#ifndef XTERM_THEME_LIST_H
#define XTERM_THEME_LIST_H
/* The themes Limpet Preferences offers: the files of the user's drawer
 * (ENVARC:Limpet/Themes) and of the program's Themes drawer, by file name.
 * A file is not read to be listed: a drawer may hold hundreds. Whether it
 * is a colour file shows when it is chosen. */
#include <stddef.h>
#define XTHEME_LIST_MAX 1024
#define XTHEME_NAME_MAX 64
typedef struct {
    char name[XTHEME_NAME_MAX];   /* the file's name: what theme= takes */
    unsigned char user;           /* from the user's drawer */
} XThemeEntry;
/* Both lists merged into out, sorted without regard to case; of two files
 * with one name the user's is kept, as Limpet finds that one first. Names
 * that are not a theme's (icons, files starting with a dot, backups ending
 * in .bak or .new) are left out. Returns the number of entries. */
unsigned theme_list_merge(XThemeEntry *out, unsigned capacity, const XThemeEntry *bundled, unsigned bundled_count,
                          const XThemeEntry *user, unsigned user_count);
/* The name as the list shows it: without .theme, .conf or .itermcolors. */
void theme_list_label(char *out, size_t size, const char *name);
/* The entry a theme= value selects, or -1: the name as given, or with one
 * of the endings, compared without case, as the file system does. */
int theme_list_find(const XThemeEntry *list, unsigned count, const char *value);
#endif
