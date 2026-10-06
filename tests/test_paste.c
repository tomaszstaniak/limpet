#include "../src/paste.h"
#include "../src/echo_session.h"
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XPaste p;
int main(void)
{
    const unsigned char *bytes=NULL; size_t n;
    const char *unicode="Zażółć 中文 😀 👨‍👩‍👧‍👦";
    assert(!paste_prepare(&p,(const unsigned char *)unicode,strlen(unicode)));
    n=paste_peek(&p,&bytes,65536); assert(n==strlen(unicode));
    assert(!memcmp(bytes,unicode,n));
    paste_consume(&p,2); assert(paste_peek(&p,&bytes,3)==2);
    assert(!memcmp(bytes,"ż",2));
    paste_consume(&p,999999); assert(!paste_peek(&p,&bytes,3));
    assert(!paste_prepare(&p,(const unsigned char *)"A\r\nB\nC\rD\t",9));
    assert(p.confirmation && !paste_peek(&p,&bytes,99));
    paste_confirm(&p); n=paste_peek(&p,&bytes,99);
    assert(n==8 && !memcmp(bytes,"A\rB\rC\rD\t",8));
    { const char *bad[]={"\033[2J","\b","\177","\302\233","\300\257","\xf0\x9f"};
      unsigned i;
      for(i=0;i<sizeof bad/sizeof bad[0];++i) {
          assert(paste_prepare(&p,(const unsigned char *)bad[i],strlen(bad[i]))==-1);
          assert(p.length==8); /* Invalid input must not replace queued data. */
      }
      assert(paste_prepare(&p,(const unsigned char *)"\0",1)==-1);
      assert(paste_prepare(&p,(const unsigned char *)"",PASTE_LIMIT+1)==-2); }
    paste_cancel(&p); assert(!p.confirmation && !paste_peek(&p,&bytes,99));
    /* Exercise real bounded session backpressure and parser, not a mock. */
    { static unsigned char input[6000]; EchoSession echo; XTermScreen s;
      XTermCell cells[16]; unsigned char out[127]; size_t used=0;
      memset(input,'x',sizeof input);
      assert(!paste_prepare(&p,input,sizeof input));
      echo_session_init(&echo); assert(!screen_init(&s,cells,16,2,8));
      while((n=paste_peek(&p,&bytes,PASTE_LIMIT))) {
          size_t accepted=echo_session_write(&echo,bytes,n);
          paste_consume(&p,accepted); used+=accepted;
          while((n=echo_session_read(&echo,out,sizeof out))) screen_write(&s,out,n);
      }
      assert(used==6000 && screen_cell(&s,1,7)=='x'); }
    /* Cancellation must never strand a UTF-8 prefix inside the parser. */
    { unsigned char input[1027]; XTermScreen s; XTermCell cells[32];
      memset(input,'a',1023); memcpy(input+1023,"😀",4);
      assert(!paste_prepare(&p,input,sizeof input));
      assert(!screen_init(&s,cells,32,2,16));
      n=paste_peek(&p,&bytes,1024); screen_write(&s,bytes,n);
      paste_consume(&p,n); paste_cancel(&p);
      assert(s.utf8_remaining==0);
      screen_write(&s,(const unsigned char *)"X",1);
      assert(screen_cell(&s,1,15)=='X'); }
    /* A clipboard carrying a fake end marker, or the C1 CSI as UTF-8, is
       refused whole, so framed text can never end its own frame; letters
       whose UTF-8 holds 0x9b (s-acute) pass unchanged. */
    { const char *fake = "abc\033[201~\ry\r";
      const char *c1 = "abc\302\233" "201~";
      const char *pl = "Za\305\274\303\263\305\202\304\207 g\304\231\305\233l\304\205";
      p.length = 0;
      assert(paste_prepare(&p, (const unsigned char *)fake, strlen(fake)) == -1 && !p.length);
      assert(paste_prepare(&p, (const unsigned char *)c1, strlen(c1)) == -1 && !p.length);
      assert(!paste_prepare(&p, (const unsigned char *)pl, strlen(pl)));
      n = paste_peek(&p, &bytes, PASTE_LIMIT);
      assert(n == strlen(pl) && !memcmp(bytes, pl, n));
      paste_cancel(&p); }
    /* Bracketed paste: the markers go out through the same partial writes
       as the text, so they are queued, not written once and forgotten. */
    { XPasteFrame f; const unsigned char *m;
      paste_frame_begin(&f, 0);
      assert(paste_frame_peek(&f, &m) == 0 && !f.open);
      paste_frame_end(&f);
      assert(paste_frame_peek(&f, &m) == 0);                  /* mode off: nothing at all */
      paste_frame_begin(&f, 1);
      assert(f.open && paste_frame_peek(&f, &m) == 6 && !memcmp(m, "\033[200~", 6));
      paste_frame_consume(&f, 2);                             /* the session took two bytes */
      assert(paste_frame_peek(&f, &m) == 4 && !memcmp(m, "200~", 4));
      paste_frame_consume(&f, 4);
      assert(paste_frame_peek(&f, &m) == 0);
      paste_frame_end(&f);
      assert(!f.open && paste_frame_peek(&f, &m) == 6 && !memcmp(m, "\033[201~", 6));
      paste_frame_end(&f);                                    /* a second end adds nothing */
      paste_frame_consume(&f, 6);
      assert(paste_frame_peek(&f, &m) == 0); }
    puts("paste tests passed (UTF-8, controls, confirmation, bounded session, safe cancel)");
    return 0;
}
