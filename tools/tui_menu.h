#ifndef XTERM_TUI_MENU_H
#define XTERM_TUI_MENU_H
#include <stddef.h>
typedef struct { unsigned selected, escape; } TuiMenu;
extern const char tui_menu_enter[],tui_menu_leave[];
/* Demo-only VT producer: -1 quit, 1 selection changed, 0 ignored. */
int tui_menu_input(TuiMenu *,unsigned char);
size_t tui_menu_draw(const TuiMenu *,char *,size_t);
#endif
