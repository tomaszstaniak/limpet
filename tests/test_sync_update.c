/* DEC 2026 synchronized output in the screen model: begin and end are seen
 * also when split across writes, and an end counts once. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[3*8];
static void text(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
int main(void) {
    static const char begin[]="\033[?2026h", end[]="\033[?2026l";
    size_t split;
    assert(!screen_init(&s,cells,24,3,8));
    assert(!s.sync_update && !s.sync_ends);
    text(begin); assert(s.sync_update && !s.sync_ends);
    text("abc"); assert(s.sync_update);
    text(end); assert(!s.sync_update && s.sync_ends==1);
    /* An end with no begin is not a frame end. */
    text(end); assert(s.sync_ends==1);
    /* A whole frame in one write: begin and end both counted. */
    text("\033[?2026hxyz\033[?2026l"); assert(!s.sync_update && s.sync_ends==2);
    for(split=1;split<sizeof begin-1;++split) {
        screen_write(&s,(const unsigned char *)begin,split);
        assert(!s.sync_update);
        screen_write(&s,(const unsigned char *)begin+split,sizeof begin-1-split);
        assert(s.sync_update);
        text(end);
    }
    /* Mixed with other private modes in one sequence. */
    text("\033[?25;2026h"); assert(s.sync_update && s.cursor_visible);
    text("\033[?2026;25l"); assert(!s.sync_update && !s.cursor_visible);
    puts("sync update: ok");
    return 0;
}
