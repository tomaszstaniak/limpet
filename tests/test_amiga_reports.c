/* The Amiga console's reports in an AROS console session (amiga_pens):
 * window bounds and cursor, with the 8-bit and the 7-bit CSI. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[40*100];
static unsigned char got[256];
static size_t got_length;
static void sink(void *c,const unsigned char *b,size_t n)
{ (void)c; assert(got_length+n<=sizeof got); memcpy(got+got_length,b,n); got_length+=n; }
static void init(unsigned rows,unsigned cols,int amiga)
{
    assert(!screen_init(&s,cells,40*100,rows,cols));
    s.amiga_pens=amiga; s.reply=sink; got_length=0;
}
static void out(const char *t,size_t n) { screen_write(&s,(const unsigned char *)t,n); }
#define REPLY(lit) reply_is(lit,sizeof lit-1)
static int reply_is(const char *want,size_t n)
{ int ok=got_length==n && !memcmp(got,want,n); got_length=0; return ok; }
int main(void)
{
    /* Window bounds: 8-bit and 7-bit queries, with and without the 0. */
    init(24,80,1);
    out("\x9b" "0 q",4); assert(REPLY("\x9b" "1;1;24;80 r"));
    out("\x9b" " q",3);  assert(REPLY("\x9b" "1;1;24;80 r"));
    out("\033[0 q",5);   assert(REPLY("\x9b" "1;1;24;80 r"));
    /* After a resize the new size. */
    assert(!screen_resize(&s,16,56));
    out("\x9b" "0 q",4); assert(REPLY("\x9b" "1;1;16;56 r"));
    /* A query split between writes, as a program may send it. */
    out("\x9b",1); out("0",1); out(" ",1); out("q",1);
    assert(REPLY("\x9b" "1;1;16;56 r"));
    /* Cursor: one-based, with the query's own introducer. */
    out("\033[3;7H",6);
    out("\x9b" "6n",3);  assert(REPLY("\x9b" "3;7R"));
    out("\033[6n",4);    assert(REPLY("\033[3;7R"));
    /* Other parameters are not the status request. */
    out("\x9b" "1 q",4); assert(got_length==0);
    /* 0x9B inside a UTF-8 sequence stays a continuation byte:
       U+049B is D2 9B. */
    out("\033[1;1H\xd2\x9b",8); assert(got_length==0);
    assert(screen_cell(&s,0,0)==0x49b);
    /* 8-bit CSI also moves the cursor and erases, as console.device does. */
    out("\x9b" "5;10H",6); assert(s.cursor_row==4 && s.cursor_column==9);
    /* A stray 0x9B in text (CP1252's single right angle quote): it starts a
       sequence as in console.device, but a line end inside still acts. */
    init(5,20,1);
    out("a\x9b\r\nnext",8);
    /* CR and LF act; 'n' is the sequence's final byte, as in console.device. */
    assert(s.cursor_row==1 && screen_cell(&s,1,0)=='e' && screen_cell(&s,0,0)=='a');
    /* 0x9B inside a sequence starts it again. */
    out("\033[\x9b" "6n",5); assert(REPLY("\x9b" "2;4R"));
    /* Line feed mode: on in an AROS console session, as console.device
       starts; CSI 20 l clears it, CSI 20 h sets it again. */
    init(5,20,1);
    out("abc\ndef",7); assert(s.cursor_row==1 && screen_cell(&s,1,0)=='d');
    out("\x9b" "20l",4); out("\nxy",3);
    assert(s.cursor_row==2 && screen_cell(&s,2,3)=='x' && screen_cell(&s,2,0)==' ');
    out("\033[20h\nz",7); assert(s.cursor_row==3 && screen_cell(&s,3,0)=='z');
    /* A reset screen follows the session again. */
    out("\x9b" "20l",4);
    assert(!screen_init(&s,cells,40*100,5,20)); s.amiga_pens=1;
    out("a\nb",3); assert(screen_cell(&s,1,0)=='b');
    /* VT sessions: off by default, as in xterm; CSI 20 h turns it on. */
    init(5,20,0);
    out("abc\ndef",7); assert(screen_cell(&s,1,3)=='d');
    out("\033[20h\nz",7); assert(screen_cell(&s,2,0)=='z');
    /* VT sessions (no amiga_pens): 0x9B is no CSI, SPACE q is no query. */
    init(24,80,0);
    out("\033[0 q",5); assert(got_length==0);
    out("\x9b" "6n",3); assert(got_length==0);
    assert(screen_cell(&s,0,0)==0xfffd);
    out("\033[6n",4); assert(REPLY("\033[1;4R"));
    puts("amiga reports PASS: line feed mode (console on, VT off, CSI 20 h/l, reset), bounds (8-bit, 7-bit, with and without 0), after resize, split query, cursor in the query's introducer, UTF-8 continuation, VT unchanged");
    return 0;
}
