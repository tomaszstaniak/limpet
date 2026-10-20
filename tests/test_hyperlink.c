#include "../src/screen_model.h"
#include "../src/hyperlink.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen s;
static XTermCell cells[10000], history[10000], alternate[10000];
static XHyperlinks links;
static XHyperlinkHit hit;
static void reset(unsigned rows, unsigned cols)
{
    assert(!screen_init(&s,cells,10000,rows,cols));
    assert(!screen_attach_history(&s,history,10000));
    assert(!screen_attach_alternate(&s,alternate,10000));
    hyperlinks_init(&links); screen_attach_links(&s,&links);
}
static void write_text(const char *p) { screen_write(&s,(const unsigned char *)p,strlen(p)); }
static void expect(unsigned r,unsigned c,const char *url)
{ assert(hyperlink_at(&s,r,c,&hit)); assert(!strcmp(hit.url,url)); }
int main(void)
{
    unsigned i; char long_url[2300];
    const char *bad[]={"file:///tmp/a","https://","https:///x","https://u@host/x",
      "https://x/\n","https://x/a b","https://x/\"","https://x/'","https://x/`",
      "https://x/$","https://x/*","https://x/\\","https://x/<","https://x/>",
      "https://x/|","https://x/\xc3\xa9","https://x/%","https://x/%0g"};
    { char path[XHYPERLINK_LIMIT];
      assert(hyperlink_file_path("file:///RAM:doc%20name.txt",path,sizeof path));
      assert(!strcmp(path,"RAM:doc name.txt"));
      assert(hyperlink_open_safe("file:///RAM:doc%20name.txt"));
    }
    { const char *sequence="\033]8;;file:///RAM:doc%20name.txt\033\\Document\033]8;;\a";
      size_t at;
      for(at=0;at<=strlen(sequence);++at) {
        reset(4,40);screen_write(&s,(const unsigned char *)sequence,at);
        write_text(sequence+at);expect(0,2,"file:///RAM:doc%20name.txt");
      }
    }
    { char path[2048];const char *invalid[]={"file://server/RAM:a","file:///RAM:a%00b","file:///RAM:a%1bb","file:///RAM:a?x","file:///RAM:a#x","file:///RAM:a%","file:///RAM:a%gg","file:///a/b","file:///:a","file:///RAM:a:b"};
      for(i=0;i<sizeof invalid/sizeof *invalid;++i) assert(!hyperlink_file_path(invalid[i],path,sizeof path));
      assert(hyperlink_file_path("file:///RAM:a%23%3f%25.txt",path,sizeof path));assert(!strcmp(path,"RAM:a#?%.txt"));
    }
    reset(4,30);
    write_text("\033]8;id=test;https://example.org/a\033"); write_text("\\A\033[0mB");
    expect(0,0,"https://example.org/a"); assert(hyperlink_contains(&s,&hit,0,1));
    write_text("\033]8;;\aC"); assert(!hyperlink_at(&s,0,2,&hit));
    write_text("\033]8;;https://wide/\a\xe7\x95\x8c");
    expect(0,3,"https://wide/"); expect(0,4,"https://wide/");
    write_text("\033[1;5H\033[K"); assert(!hyperlink_at(&s,0,3,&hit));
    reset(4,30); write_text("\033]8;;https://primary/\aP\033[?1049h");
    write_text("\033]8;;https://alternate/\aA"); expect(0,0,"https://alternate/");
    write_text("\033[?1049lQ"); expect(0,1,"https://primary/");
    reset(4,30); write_text("See https://example.org/a(b)). ");
    expect(0,5,"https://example.org/a(b)"); assert(!hyperlink_at(&s,0,28,&hit));
    reset(3,10); write_text("https://example.org/path ");
    expect(1,3,"https://example.org/path"); assert(hyperlink_contains(&s,&hit,0,0));
    write_text("\r\n\r\n"); screen_scroll_view(&s,2);
    expect(0,0,"https://example.org/path");
    reset(4,10); write_text("https://a\r\nbbbbb "); expect(0,0,"https://a");
    assert(!hyperlink_at(&s,1,1,&hit));
    reset(4,30); write_text("\033]8;;file:///tmp/x\ahttps://fake/\033]8;;\a");
    assert(!hyperlink_at(&s,0,2,&hit));
    reset(4,30); write_text("\033]8;;https://first/\aX\033]8;;\a");
    for(i=0;i<64;++i) write_text("\033]8;;https://new/\a\033]8;;\a");
    assert(!hyperlink_at(&s,0,0,&hit));
    reset(4,30); write_text("\033]8;;https://x/\001bad\ahttps://fake/ ");
    assert(!hyperlink_at(&s,0,2,&hit)); assert(screen_cell(&s,0,0)=='h');
    memset(long_url,'a',sizeof long_url); memcpy(long_url,"https://",8); long_url[2299]=0;
    reset(3,100); write_text("\033]8;;"); write_text(long_url); write_text("\ahttps://fake/ ");
    assert(!hyperlink_at(&s,0,2,&hit));
    reset(30,100); write_text(long_url); assert(!hyperlink_at(&s,0,2,&hit));
    assert(!hyperlink_at(&s,22,90,&hit));
    /* Token exhaustion must never make an old cell point to a new target. */
    reset(4,30); write_text("\033]8;;https://old/\aO\033]8;;\a");
    links.next_token=UINT32_MAX-1;
    write_text("\033]8;;https://last/\aL\033]8;;\a");
    write_text("\033]8;;https://exhausted/\ahttps://fake/");
    expect(0,0,"https://old/"); expect(0,1,"https://last/");
    assert(!hyperlink_at(&s,0,3,&hit));
    /* Non-ASCII continuation must never turn a prefix into a different URL,
     * even when a wide wrap hides it behind a terminal padding cell. */
    reset(4,10); write_text("https://a\xe7\x95\x8c");
    assert(!hyperlink_at(&s,0,0,&hit));
    assert(!hyperlink_at(&s,0,9,&hit)); assert(!hyperlink_at(&s,1,0,&hit));
    reset(4,40); write_text("https://example.org/caf\xc3\xa9");
    assert(!hyperlink_at(&s,0,0,&hit));
    reset(4,40); write_text("https://example.org/cafe\xcc\x81");
    assert(!hyperlink_at(&s,0,0,&hit));
    reset(4,40); write_text("https://example.org/ \xe7\x95\x8c");
    expect(0,0,"https://example.org/");
    reset(4,40); write_text("\033]8;;https://example.org/\a\xe7\x95\x8c");
    expect(0,0,"https://example.org/"); expect(0,1,"https://example.org/");
    /* Copy preserves literal unsafe launch characters, without decoding. */
    reset(4,40); write_text("\033]8;;https://x/a'$*|%22\aX");
    expect(0,0,"https://x/a'$*|%22"); assert(!hyperlink_open_safe(hit.url));
    reset(4,40);
    { const char *p="\033]8;id=fragment;https://fragment/\033\\X\033]8;;\033\\Y";
      while(*p) { screen_write(&s,(const unsigned char *)p,1); ++p; } }
    expect(0,0,"https://fragment/"); assert(!hyperlink_at(&s,0,1,&hit));
    /* A stray ESC followed by BEL is malformed, rather than a valid ST. */
    reset(4,40); write_text("\033]8;;https://x/\033\ahttps://fake/");
    assert(!hyperlink_at(&s,0,0,&hit));
    /* The maximum target works; the next byte must reject the entire URL. */
    long_url[2047]=0;
    reset(30,100); write_text(long_url); expect(0,1,long_url);
    reset(4,40); write_text("\033]8;;"); write_text(long_url); write_text("\aX");
    expect(0,0,long_url);
    long_url[2047]='a'; long_url[2048]=0;
    reset(30,100); write_text(long_url); assert(!hyperlink_at(&s,0,1,&hit));
    reset(4,40); write_text("\033]8;;"); write_text(long_url); write_text("\ahttps://fake/");
    assert(!hyperlink_at(&s,0,1,&hit));
    reset(4,40); write_text("\033]8;"); write_text(long_url);
    write_text(";https://x/\ahttps://fake/\033]8;;\a ");
    assert(!hyperlink_at(&s,0,1,&hit));
    reset(4,40); write_text("https://old/ "); expect(0,1,"https://old/");
    write_text("\rhttps://new/ "); assert(!hyperlink_contains(&s,&hit,0,1));
    reset(4,40); write_text("\033]8;;https://old/\aX"); expect(0,0,"https://old/");
    write_text("\033[2J"); assert(!hyperlink_contains(&s,&hit,0,0));
    assert(!hyperlink_at(&s,0,1,&hit));
    assert(hyperlink_open_safe("https://example.org/a?b=c&d=%22"));
    for(i=0;i<sizeof bad/sizeof bad[0];++i) assert(!hyperlink_open_safe(bad[i]));
    puts("hyperlinks: ok"); return 0;
}
