#include "../src/aros_session.h"
#include "../src/screen_model.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ArosSession session;
static XTermScreen screen;
static XTermCell cells[3200],alternate[3200],history[800];
static void fail(int line) {
    fprintf(stderr,"TUI assertion at %d\n",line);
    aros_session_stop(&session);
    while(session.task_id) { aros_session_poll(&session); Delay(1); }
    while(aros_session_dispose(&session)) { aros_session_poll(&session); Delay(1); }
    exit(20);
}
#define CHECK(x) do { if(!(x)) fail(__LINE__); } while(0)
static void pump(void) {
    unsigned char b[4096]; size_t n;
    aros_session_poll(&session);
    while((n=aros_session_read(&session,b,sizeof b))) screen_write(&screen,b,n);
}
static int ready(unsigned selection) {
    return screen.alternate_active && screen_cell(&screen,11,13)=='1'+selection;
}
static void await_selection(unsigned selection) {
    unsigned ticks;
    for(ticks=0;ticks<500;++ticks) { pump(); if(ready(selection)) return; Delay(1); }
    fail(__LINE__);
}
static void finish(void) {
    unsigned ticks;
    for(ticks=0;ticks<500 && !session.exited;++ticks) { pump(); Delay(1); }
    pump(); CHECK(session.exited && !session.result && !session.unsupported_action);
    CHECK(!screen.alternate_active && screen.cursor_visible);
    CHECK(screen_cell(&screen,0,0)=='S' && screen_cell(&screen,0,6)=='B');
    CHECK(!session.line.raw && !session.handles);
    CHECK(!aros_session_dispose(&session));
}
int main(void) {
    char peer[512],command[560]; unsigned run;
    CHECK(!aros_session_peer_command(peer,sizeof peer));
    snprintf(command,sizeof command,"%s --tui",peer);
    for(run=0;run<3;++run) {
        CHECK(!screen_init(&screen,cells,3200,24,80));
        CHECK(!screen_attach_alternate(&screen,alternate,3200));
        CHECK(!screen_attach_history(&screen,history,800));
        screen_write(&screen,(const unsigned char *)"SHELL BEFORE\r\n",14);
        CHECK(!aros_session_start_console(&session,command));
        await_selection(0); CHECK(session.line.raw && !screen.cursor_visible);
        CHECK(screen_cell(&screen,9,7)==0x4e2d);
        if(run==0) {
            /* Fragment input across Read calls, exactly as a real key stream. */
            CHECK(aros_session_write(&session,(const unsigned char *)"\033",1)==1);
            pump(); CHECK(aros_session_write(&session,(const unsigned char *)"[B",2)==2);
            await_selection(1);
            CHECK(screen_view_cell(&screen,6,2).bg==4);
            CHECK(aros_session_write(&session,(const unsigned char *)"\033[B",3)==3);
            await_selection(2);
            CHECK(aros_session_write(&session,(const unsigned char *)"\033[A",3)==3);
            await_selection(1);
            CHECK(!screen_resize(&screen,20,72));
            CHECK(aros_session_write(&session,(const unsigned char *)"q",1)==1);
        } else if(run==1) aros_session_eof(&session);
        else CHECK(aros_session_write(&session,(const unsigned char *)"\003",1)==1);
        finish();
    }
    puts("native TUI PASS: alternate screen, arrows, Unicode output, resize, q/EOF/raw Ctrl-C, primary restoration, mode and task cleanup");
    return 0;
}
