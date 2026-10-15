#include "../src/output_search.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[16*40], history[64*40], spare[64*40], alternate[16*40];
static XOutputSearch q;
static unsigned char marks[16*40];
static void init(unsigned rows,unsigned cols,unsigned history_rows)
{
    assert(!screen_init(&s,cells,16*40,rows,cols));
    assert(!screen_attach_history(&s,history,(size_t)history_rows*cols));
    assert(!screen_attach_reflow(&s,spare,(size_t)history_rows*cols));
    assert(!screen_attach_alternate(&s,alternate,16*40));
}
static void out(const char *t) { screen_write(&s,(const unsigned char *)t,strlen(t)); }
static void type(const char *t) { assert(output_search_insert(&q,&s,t,strlen(t))); }
/* The text from the current match's start, cells long, as shown by marks. */
static unsigned current_cells(void)
{
    unsigned i,n=0;
    output_search_marks(&q,&s,marks,sizeof marks);
    for(i=0;i<s.rows*s.columns;++i) n+=marks[i]==2;
    return n;
}
static char label[96];
static const char *lab(void) { output_search_label(&q,label,sizeof label); return label; }
int main(void)
{
    /* Basic: newest match at or above the view's bottom first; Up older. */
    init(3,10,8);
    out("alpha\r\nbeta\r\nALPHA two\r\ngamma\r\nalphabet\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    assert(!strcmp(lab(),"Find: type to search, Esc closes"));
    type("alp");
    assert(q.found && q.total==3 && q.index==3);
    assert(!strcmp(lab(),"Find: alp - 3 of 3"));
    assert(output_search_step(&q,&s,1) && q.index==2);   /* ALPHA: ASCII case folded */
    assert(output_search_step(&q,&s,1) && q.index==1);
    assert(!output_search_step(&q,&s,1) && q.index==1); /* the oldest: stays */
    /* The oldest is in the history: the view went there. */
    assert(s.view_offset>0 && current_cells()==3);
    assert(output_search_step(&q,&s,0) && q.index==2);
    assert(output_search_step(&q,&s,0) && q.index==3);
    assert(!output_search_step(&q,&s,0));
    /* Longer query: still the result shown, if it matches. */
    type("habet");
    assert(q.found && q.total==1 && q.index==1 && current_cells()==8);
    type("x");
    assert(!q.found && !strcmp(lab(),"Find: alphabetx - not found"));
    assert(output_search_backspace(&q,&s) && q.found && q.total==1);
    /* Esc: the view from before (the bottom). */
    output_search_close(&q,&s,1);
    assert(!q.open && s.view_offset==0);

    /* Invalid input changes nothing. */
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    assert(!output_search_insert(&q,&s,"a\033",2));
    assert(!output_search_insert(&q,&s,"\xc3",1));
    assert(q.query_length==0);
    { char big[XSEARCH_QUERY_MAX]; memset(big,'a',sizeof big);
      assert(!output_search_insert(&q,&s,big,sizeof big)); }
    output_search_close(&q,&s,0);

    /* A soft-wrapped line is one line: a match across the wrap. */
    init(4,6,8);
    out("abcdefghij\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("efgh");
    assert(q.found && q.total==1 && current_cells()==4);
    output_search_marks(&q,&s,marks,sizeof marks);
    assert(marks[4]==2 && marks[5]==2 && marks[6]==2 && marks[7]==2 && marks[8]==0);
    output_search_close(&q,&s,1);
    /* A hard line end is not crossed. */
    init(4,6,8);
    out("abcdef\r\nghij\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("efgh");
    assert(!q.found && q.total==0);
    output_search_close(&q,&s,1);

    /* Wide and combined text: a match is whole cells. */
    init(3,10,8);
    out("x中文y e\xcc\x81 z\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("中文");
    assert(q.found && current_cells()==2);
    output_search_marks(&q,&s,marks,sizeof marks);
    assert(marks[0]==0 && marks[1]==2 && marks[3]==2);  /* lead cells; columns 1 and 3 */
    output_search_backspace(&q,&s); output_search_backspace(&q,&s);
    type("e");                                       /* e alone is not e + U+0301 */
    assert(!q.found);
    type("\xcc\x81");
    assert(q.found && q.total==1);
    output_search_close(&q,&s,1);
    /* Not normalised: precomposed é is different text. */
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("\xc3\xa9");
    assert(!q.found);
    output_search_close(&q,&s,1);

    /* Arriving output does not move the view or the result. */
    init(3,10,20);
    out("one\r\nneedle 1\r\ntwo\r\nthree\r\nfour\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("needle");
    assert(q.found && q.total==1 && s.view_offset>0);
    { unsigned long line=q.line; unsigned offset=s.view_offset;
      out("five\r\nneedle 2\r\nsix\r\n");
      assert(!output_search_follow(&q,&s));
      assert(q.line==line && q.index==1 && q.total==2);
      assert(s.view_offset==offset+3 && current_cells()==6); }
    /* The newer one is reachable. */
    assert(output_search_step(&q,&s,0) && q.index==2);
    /* Esc after output: back to the bottom, where the view was. */
    output_search_close(&q,&s,1);
    assert(s.view_offset==0);

    /* At the bottom, the live view is held too while the search is open. */
    init(3,10,20);
    out("a\r\nb\r\nneedle\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("needle");
    assert(q.found && s.view_offset==0);
    out("c\r\nd\r\n");
    output_search_follow(&q,&s);
    assert(s.view_offset==2 && current_cells()==6);
    output_search_close(&q,&s,1);
    assert(s.view_offset==0);

    /* A scrolled-back view is restored to the same text by Esc. */
    init(3,10,20);
    out("1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\nkey\r\n8\r\n");
    screen_scroll_view(&s,2);
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("1");
    assert(q.found && s.view_offset>2);
    out("9\r\n");
    output_search_follow(&q,&s);
    output_search_close(&q,&s,1);
    assert(s.view_offset==3);

    /* The oldest lines fall out: a result there goes to the next one. */
    init(2,10,5);
    out("hit a\r\nx\r\nhit b\r\ny\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("hit");
    assert(q.total==2);
    assert(output_search_step(&q,&s,1) && q.index==1);
    out("p\r\nq\r\nr\r\n");                  /* history of 5: only "hit a" is gone */
    output_search_follow(&q,&s);
    assert(q.found && q.total==1 && q.index==1);
    output_search_marks(&q,&s,marks,sizeof marks);
    output_search_close(&q,&s,1);

    /* Overwritten text: the result found again, the view kept. */
    init(3,10,8);
    out("abc\r\nabc\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("abc");
    assert(q.total==2 && q.index==2);
    out("\033[2;1Hzzz");
    output_search_follow(&q,&s);
    assert(q.found && q.total==1 && q.index==1);
    output_search_close(&q,&s,1);

    /* Reflow: the same result counted from the newest. */
    init(4,10,30);
    out("tok 1 aaaaaaaa\r\ntok 2\r\ntok 3 bbbbbbbbbbbb\r\ntok 4\r\nend\r\n");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("tok");
    assert(q.total==4 && q.index==4);
    assert(output_search_step(&q,&s,1) && output_search_step(&q,&s,1) && q.index==2);
    assert(!screen_resize(&s,4,6));
    assert(!output_search_follow(&q,&s));
    assert(q.found && q.total==4 && q.index==2);
    { /* and it is "tok 2": the cells after it */
      unsigned long base=s.history_pushed-s.history_count;
      const XTermCell *row=screen_line_row(&s,(unsigned)(q.line-base));
      assert(row && row[q.column+4].ch=='2'); }
    assert(current_cells()==3);
    assert(!screen_resize(&s,4,10));
    output_search_follow(&q,&s);
    assert(q.index==2 && q.total==4);
    output_search_close(&q,&s,1);
    assert(s.view_offset==0);

    /* The alternate screen: not searched; entering it closes the search. */
    init(3,10,8);
    out("text\r\n\033[?1049h");
    assert(output_search_open(&q,&s)==XSEARCH_ALTERNATE && !q.open);
    out("\033[?1049l");
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("text");
    out("\033[?1049h");
    assert(output_search_follow(&q,&s)==1 && !q.open);
    out("\033[?1049l");

    /* Label is cut at a scalar. */
    init(3,10,8);
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("żółwżółw");
    { char small[20]; size_t n=output_search_label(&q,small,sizeof small);
      assert(n<sizeof small && !memcmp(small,"Find: ",6));
      assert(((unsigned char)small[n-1]&0xc0)!=0x80 || n>6); }
    output_search_close(&q,&s,0);

    /* A match that starts above the view is marked where it shows. */
    init(2,4,8);
    out("abcdefgh\r\n");             /* rows: abcd efgh (wrapped), then the cursor row */
    assert(output_search_open(&q,&s)==XSEARCH_OPENED);
    type("cdef");
    screen_scroll_view(&s,-(int)s.view_offset);
    assert(s.history_count==1 && s.view_offset==0);
    output_search_marks(&q,&s,marks,sizeof marks);
    assert(marks[0]==2 && marks[1]==2 && marks[2]==0);
    output_search_close(&q,&s,0);
    puts("output search tests PASS");
    return 0;
}
