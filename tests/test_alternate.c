#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[128], alternate[128], history[32], saved[128];
static void text(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
static void init(void) {
    assert(!screen_init(&s,cells,128,3,8));
    assert(!screen_attach_history(&s,history,32));
    assert(!screen_attach_alternate(&s,alternate,128));
}
int main(void) {
    static const unsigned char enter[]="\033[?1049h", leave[]="\033[?1049l";
    size_t split;
    /* A missing switch, destructive re-entry or lost cursor/style fails here. */
    for(split=0;split<sizeof enter;++split) {
        unsigned r,c,wrap;
        init(); text("old\r\none\r\ntwo\r\n\033[31m中é😀");
        r=s.cursor_row; c=s.cursor_column; wrap=s.wrap_pending;
        assert(s.history_count==1);
        memcpy(saved,cells,24*sizeof *cells);
        screen_write(&s,enter,split);
        screen_write(&s,enter+split,sizeof enter-1-split);
        assert(s.alternate_active && s.history_count==0 && s.view_offset==0);
        assert(screen_cell(&s,0,0)==' ');
        text("\033[Hmenu\033[?1049h");
        assert(screen_cell(&s,0,0)=='m');
        assert(screen_attach_alternate(&s,alternate,128)==-1);
        text("\r\n1\r\n2\r\n3\r\n4");
        assert(!s.history_count);
        screen_scroll_view(&s,9); assert(!s.view_offset);
        screen_write(&s,leave,split);
        screen_write(&s,leave+split,sizeof leave-1-split);
        assert(!s.alternate_active && s.history_count==1);
        assert(s.cursor_row==r && s.cursor_column==c && s.wrap_pending==wrap);
        assert(s.style.fg==1 && !memcmp(saved,cells,24*sizeof *cells));
        assert(screen_line_cell(&s,0,0).ch=='o');
        text("\033[?1049l"); assert(!memcmp(saved,cells,24*sizeof *cells));
    }
    /* Resize both visible TUI and saved Shell; never restore an orphan half. */
    init(); text("123456中\033[?1049h\033[Habcdef中");
    assert(!screen_resize(&s,2,7));
    assert(screen_cell(&s,0,6)==' ');
    text("\033[?1049l");
    assert(screen_cell(&s,0,0)=='1' && screen_cell(&s,0,6)==' ');
    assert(s.cursor_column<7 && s.cursor_row<2 && !s.wrap_pending);
    text("\033[?1049h"); assert(!screen_resize(&s,4,10));
    text("\033[?1049l"); assert(screen_cell(&s,0,0)=='1');
    assert(screen_cell(&s,3,9)==' ');
    /* Private mode parsing must never turn private erase/SGR into normal CSI. */
    init(); text("keep\033[?2J\033[?31m");
    assert(screen_cell(&s,0,0)=='k' && s.style.fg==7);
    assert(s.cursor_visible);
    text("\033[?25l"); assert(!s.cursor_visible);
    text("\033[25h\033[0?25h\033[??25h"); assert(!s.cursor_visible);
    text("\033[?25;9999h"); assert(s.cursor_visible);
    text("\033[?1049;25h"); assert(s.alternate_active && s.cursor_visible);
    text("\033[?1049;25l"); assert(!s.alternate_active && !s.cursor_visible);
    assert(screen_attach_alternate(&s,alternate,127)==-1);
    assert(screen_attach_alternate(&s,cells,128)==-1);
    /* No attached storage means an unsupported mode, not lost primary text. */
    assert(!screen_init(&s,cells,128,3,8)); text("keep\033[?1049h");
    assert(!s.alternate_active && screen_cell(&s,0,0)=='k');
    puts("alternate screen PASS: restore, split CSI, Unicode, history isolation, resize, cursor visibility, bounds");
    return 0;
}
