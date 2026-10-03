#include "../tools/tui_menu.h"
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermCell cells[1920],alt[1920];
static XTermScreen screen;
static void feed(const char *p) { screen_write(&screen,(const unsigned char *)p,strlen(p)); }
static void draw(const TuiMenu *menu) {
    char frame[1024]; size_t n=tui_menu_draw(menu,frame,sizeof frame);
    assert(n && n<sizeof frame); screen_write(&screen,(const unsigned char *)frame,n);
}
int main(void) {
    TuiMenu menu={0,0}; char tiny[2];
    assert(!screen_init(&screen,cells,1920,24,80));
    assert(!screen_attach_alternate(&screen,alt,1920));
    feed("SHELL BEFORE\r\n"); feed(tui_menu_enter); draw(&menu);
    assert(screen.alternate_active && !screen.cursor_visible);
    assert(screen_cell(&screen,9,7)==0x4e2d);
    { unsigned col,emoji=0,family=0;
      for(col=0;col<80;++col) {
          XTermCell cell=screen_view_cell(&screen,9,col);
          if(cell.ch==0x1f600) emoji=1;
          if(cell.ch==0x1f468 && cell.length==7) family=1;
      }
      assert(emoji && family);
    }
    assert(screen_cell(&screen,11,13)=='1');
    assert(screen_view_cell(&screen,5,2).bg==4);
    assert(!tui_menu_input(&menu,27)); assert(!tui_menu_input(&menu,'['));
    assert(tui_menu_input(&menu,'B')==1); draw(&menu);
    assert(screen_cell(&screen,11,13)=='2');
    assert(screen_view_cell(&screen,6,2).bg==4);
    assert(screen_view_cell(&screen,5,2).bg==0);
    assert(!tui_menu_input(&menu,'B') && menu.selected==1);
    assert(!tui_menu_input(&menu,27)); assert(!tui_menu_input(&menu,'['));
    assert(tui_menu_input(&menu,'A')==1 && menu.selected==0);
    assert(!tui_menu_input(&menu,27)); assert(!tui_menu_input(&menu,'['));
    assert(tui_menu_input(&menu,'A')==1 && menu.selected==2);
    assert(tui_menu_input(&menu,'q')==-1 && tui_menu_input(&menu,3)==-1);
    assert(!tui_menu_draw(&menu,tiny,sizeof tiny));
    feed(tui_menu_leave);
    assert(!screen.alternate_active && screen.cursor_visible);
    assert(screen_cell(&screen,0,0)=='S' && screen_cell(&screen,0,6)=='B');
    puts("TUI menu PASS: actual VT frames, fragmented arrows, selection, raw Ctrl-C, restoration, bounds");
    return 0;
}
