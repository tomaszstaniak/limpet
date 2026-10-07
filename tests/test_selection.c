#include "../src/selection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[64], history[64];
static XSelection sel;
static void init(unsigned rows,unsigned cols,const char *text)
{
    assert(!screen_init(&s,cells,64,rows,cols));
    assert(!screen_attach_history(&s,history,64));
    memset(&sel,0,sizeof sel);
    screen_write(&s,(const unsigned char *)text,strlen(text));
}
static void expect(const char *text)
{
    unsigned char out[256]; size_t n=999;
    assert(!selection_text(&sel,&s,out,sizeof out,&n));
    assert(n==strlen(text)); assert(!memcmp(out,text,n));
}
int main(void)
{
    /* Dropping a continuation endpoint would lose the complete cluster. */
    init(3,8,"A中é😀");
    { XSelectionGrid grid={18,36,8,17}; unsigned row=999,col=999;
      assert(selection_hit(&s,&grid,34,53,0,&row,&col));
      assert(row==1 && col==2);
      assert(!selection_hit(&s,&grid,17,36,0,&row,&col));
      assert(selection_hit(&s,&grid,-10,999,1,&row,&col));
      assert(row==2 && col==0); }
    selection_start(&sel,&s,0,2); selection_extend(&sel,&s,0,5);
    expect("中é😀");
    assert(selection_contains(&sel,&s,0,1));
    assert(selection_contains(&sel,&s,0,5));
    assert(!selection_contains(&sel,&s,0,0));
    selection_start(&sel,&s,0,5); selection_extend(&sel,&s,0,2);
    expect("中é😀");
    init(2,8,"👨‍👩‍👧‍👦 🇵🇱");
    selection_start(&sel,&s,0,1); selection_extend(&sel,&s,0,4);
    expect("👨‍👩‍👧‍👦 🇵🇱");
    init(3,4,"abcdef\r\nX  ");
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,2,3);
    expect("abcdef\nX  ");
    /* A wide glyph moved to the next row must not introduce a fake space. */
    init(3,4,"abc中Z");
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,1,3);
    expect("abc中Z");
    assert(!screen_resize(&s,2,4));
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,1,3);
    expect("abc中Z"); /* Height-only resize must preserve soft-wrap links. */
    init(2,4,"abcd\r\nEF");
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,1,1);
    expect("abcd\nEF");
    init(2,16,"A\tB");
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,0,8);
    expect("A       B");
    init(2,4,"abcdefghij");
    screen_scroll_view(&s,1);
    selection_start(&sel,&s,0,0); selection_extend(&sel,&s,1,3);
    expect("abcdefgh");
    screen_scroll_view(&s,-1);
    selection_extend(&sel,&s,1,1); expect("abcdefghij");
    { unsigned char out[2]={0xaa,0xbb}; size_t n=99;
      assert(selection_text(&sel,&s,out,2,&n)==-2);
      assert(!n && out[0]==0xaa && out[1]==0xbb); }
    sel.active=0;
    { unsigned char out[8]; size_t n=99;
      assert(selection_text(&sel,&s,out,8,&n)==-1 && !n); }
    /* L6 (hardcode): a selection stays when output leaves its cells as
       they were and nothing scrolled; it goes when they change, on a
       scroll, or when the screen is cleared. */
    {
        unsigned char before[256]; size_t bn=0; unsigned long sc;
        init(4,10,"alpha\r\nbravo\r\ncharlie");
        selection_start(&sel,&s,1,0); selection_extend(&sel,&s,1,4); sel.active=1;
        assert(!selection_text(&sel,&s,before,sizeof before,&bn));
        sc=s.scroll_count;
        screen_write(&s,(const unsigned char *)"\033[4;1Hst 12 s",strlen("\033[4;1Hst 12 s"));   /* another row */
        assert(selection_keep(&sel,&s,before,bn,sc));
        screen_write(&s,(const unsigned char *)"\033[2;1Hxx",strlen("\033[2;1Hxx"));              /* its own row */
        assert(!selection_keep(&sel,&s,before,bn,sc));
        init(4,10,"alpha\r\nbravo\r\ncharlie");
        selection_start(&sel,&s,1,0); selection_extend(&sel,&s,1,4); sel.active=1;
        assert(!selection_text(&sel,&s,before,sizeof before,&bn));
        sc=s.scroll_count;
        screen_write(&s,(const unsigned char *)"\033[4;1H\r\n\r\n",strlen("\033[4;1H\r\n\r\n"));       /* scrolls */
        assert(!selection_keep(&sel,&s,before,bn,sc));
        init(4,10,"alpha\r\nbravo\r\ncharlie");
        selection_start(&sel,&s,1,0); selection_extend(&sel,&s,1,4); sel.active=1;
        assert(!selection_text(&sel,&s,before,sizeof before,&bn));
        sc=s.scroll_count;
        screen_write(&s,(const unsigned char *)"\033[2J",strlen("\033[2J"));                 /* cleared */
        assert(!selection_keep(&sel,&s,before,bn,sc));
    }
    /* a selection of more than 1 KB is kept too */
    {
        static XTermCell big[40*80], bighist[40*80];
        static unsigned char before[8192]; size_t bn=0; unsigned long sc; int r;
        assert(!screen_init(&s,big,40*80,40,80));
        assert(!screen_attach_history(&s,bighist,40*80));
        memset(&sel,0,sizeof sel);
        for(r=0;r<30;++r) screen_write(&s,(const unsigned char *)"0123456789012345678901234567890123456789012345678901234567890123456789\r\n",72);
        selection_start(&sel,&s,0,0); selection_extend(&sel,&s,29,60); sel.active=1;
        assert(!selection_text(&sel,&s,before,sizeof before,&bn) && bn>1024);
        sc=s.scroll_count;
        screen_write(&s,(const unsigned char *)"\033[40;1Hst",strlen("\033[40;1Hst"));
        assert(selection_keep(&sel,&s,before,bn,sc));
    }
    puts("selection tests passed (clusters, wraps, history, bounds)");
    return 0;
}
