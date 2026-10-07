#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermCell cells[300*25];
static void feed(XTermScreen *s,const char *p) {
    while(*p) screen_write(s,(const unsigned char *)p++,1);
}
int main(void) {
    XTermScreen s; unsigned char out[64]; size_t n;
    assert(!screen_init(&s,cells,300*25,25,300));
    /* L5 (hardcode): wheel reports without mouse tracking, private mode
       2100. Only wheel steps, always SGR; buttons stay with the terminal,
       so plain drag still selects. */
    assert(!screen_wheel_encode(&s,1,3,4,0,out,sizeof out));
    feed(&s,"\033[?2100h");
    assert(s.wheel_report && !s.mouse_tracking);
    n=screen_wheel_encode(&s,1,3,4,0,out,sizeof out);
    assert(n==10 && !memcmp(out,"\033[<64;4;5M",10));
    n=screen_wheel_encode(&s,0,3,4,16,out,sizeof out);
    assert(n==10 && !memcmp(out,"\033[<81;4;5M",10));
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out)); /* buttons: not reported */
    assert(!screen_wheel_encode(&s,1,300,0,0,out,sizeof out));
    assert(!screen_wheel_encode(&s,1,0,0,0,out,4));
    feed(&s,"\033[?2100l");
    assert(!s.wheel_report && !screen_wheel_encode(&s,1,3,4,0,out,sizeof out));
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out));
    feed(&s,"\033[?1006h"); /* Encoding alone must not capture the mouse. */
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out));
    feed(&s,"\033[?1000h");
    n=screen_mouse_encode(&s,0,299,24,0,0,out,sizeof out);
    assert(n==12 && !memcmp(out,"\033[<0;300;25M",12));
    n=screen_mouse_encode(&s,2,1,2,24,1,out,sizeof out);
    assert(n==10 && !memcmp(out,"\033[<26;2;3m",10));
    n=screen_mouse_encode(&s,0,0,0,4,1,out,sizeof out);
    assert(n==9 && !memcmp(out,"\033[<4;1;1m",9));
    n=screen_mouse_encode(&s,64,0,0,0,0,out,sizeof out);
    assert(n==10 && !memcmp(out,"\033[<64;1;1M",10));
    assert(!screen_mouse_encode(&s,64,0,0,0,1,out,sizeof out));
    assert(!screen_mouse_encode(&s,0,300,0,0,0,out,sizeof out));
    memset(out,0xa5,sizeof out);
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,2) && out[0]==0xa5);
    feed(&s,"\033[?1006l");
    n=screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out);
    assert(n==6 && !memcmp(out,"\033[M !!",6));
    n=screen_mouse_encode(&s,0,0,0,0,1,out,sizeof out);
    assert(n==6 && !memcmp(out,"\033[M#!!",6));
    assert(!screen_mouse_encode(&s,0,223,0,0,0,out,sizeof out));
    feed(&s,"\033[?1000l");
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out));
    feed(&s,"\033[1000h\033[?1000 h\033[?1003h");
    assert(!s.mouse_tracking); /* No accidental opt-in from unsupported modes. */
    feed(&s,"\033[?1000;1006h");
    assert(s.mouse_tracking && s.mouse_sgr);
    assert(!screen_mouse_encode(&s,32,4,5,0,0,out,sizeof out));
    feed(&s,"\033[?1002h");
    {
        unsigned generation=s.mouse_generation;
        feed(&s,"\033[?1002h");
        assert(s.mouse_generation==generation);
        screen_write(&s,(const unsigned char *)"\033[?1002l\033[?1002h",16);
        assert(s.mouse_tracking==1002 && s.mouse_generation==generation+2);
    }
    n=screen_mouse_encode(&s,32,4,5,0,0,out,sizeof out);
    assert(n==10 && !memcmp(out,"\033[<32;5;6M",10));
    assert(!screen_mouse_encode(&s,35,4,5,0,0,out,sizeof out));
    feed(&s,"\033[?1002l");
    assert(!screen_mouse_encode(&s,0,0,0,0,0,out,sizeof out));
    assert(!screen_mouse_encode(&s,3,0,0,0,0,out,sizeof out));
    assert(!screen_mouse_encode(&s,0,0,0,32,0,out,sizeof out));
    assert(!screen_init(&s,cells,300*25,25,300));
    assert(!s.mouse_tracking && !s.mouse_sgr);
    puts("mouse PASS: opt-in, SGR, legacy, modifiers, release, wheel, bounds");
    return 0;
}
