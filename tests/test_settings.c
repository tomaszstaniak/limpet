#include "../src/settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XSettings s;
static int parse(const char *text, const char **why)
{ return settings_parse(&s,text,strlen(text),why); }
static void rejects(const char *text, int line, const char *reason)
{
    XSettings before; const char *why;
    settings_default(&s); before=s;
    assert(parse(text,&why)==line);
    assert(why && strstr(why,reason));
    assert(!memcmp(&before,&s,sizeof s));
}
int main(void)
{
    const char *why;
    settings_default(&s);
    assert(s.font_size==13 && s.scrollback==256 && s.history==32 && !s.font[0] && !s.theme[0] && !s.given);
    /* Empty file and comments only: the defaults. */
    assert(!parse("",&why) && !s.given);
    assert(!parse("# nothing\n\n   \n",&why) && !s.given);
    /* Everything, with CR LF, spaces and a path with spaces. */
    assert(!parse("# Limpet\r\nfont = RAM Disk:fonts/Mono Font.ttf \r\nfont_size=16\r\n"
                  "theme=SYS:Utilities/Limpet/Themes/light.theme\r\nscrollback=1000\r\nhistory=100",&why));
    assert(!strcmp(s.font,"RAM Disk:fonts/Mono Font.ttf") && s.font_size==16);
    assert(!strcmp(s.theme,"SYS:Utilities/Limpet/Themes/light.theme"));
    assert(s.scrollback==1000 && s.history==100);
    assert(s.given==(XSET_FONT|XSET_FONT_SIZE|XSET_THEME|XSET_SCROLLBACK|XSET_HISTORY));
    /* Bounds are inclusive. */
    settings_default(&s);
    assert(!parse("font_size=8\nscrollback=100\nhistory=1\n",&why) && s.font_size==8 && s.scrollback==100 && s.history==1);
    assert(!parse("font_size=20\nscrollback=1000\nhistory=500\n",&why) && s.font_size==20);
    /* Rejected whole, with the line. */
    rejects("history=10\nfont_size=7\n",2,"8 to 20");
    rejects("font_size=21",1,"8 to 20");
    rejects("scrollback=99",1,"100 to 1000");
    rejects("scrollback=1001",1,"100 to 1000");
    rejects("history=0",1,"1 to 500");
    rejects("history=501",1,"1 to 500");
    rejects("history=-5",1,"1 to 500");
    rejects("history=1e3",1,"1 to 500");
    rejects("history=",1,"1 to 500");
    rejects("history=99999999999",1,"1 to 500");
    rejects("colour=#ffffff",1,"unknown key");
    rejects("Font=x",1,"unknown key");
    rejects("font\n",1,"no '='");
    rejects("theme=a\ntheme=b\n",2,"twice");
    rejects("font=\n",1,"empty");
    rejects("# ok\nscrollback=300\nhistory=40\nbad line\n",4,"no '='");
    { char big[XSETTINGS_FILE_MAX+2]; memset(big,'#',sizeof big); big[sizeof big-1]=0;
      rejects(big,1,"4096"); }
    { char line[300]="font="; memset(line+5,'a',260); line[265]=0; rejects(line,1,"255"); }
    rejects("font=a\001b\n",1,"text");
    { XSettings before; settings_default(&s); before=s;
      assert(settings_parse(&s,"history=5\0x",11,&why)==1 && !memcmp(&before,&s,sizeof s)); }
    puts("settings PASS: defaults, all keys, CR LF, spaces, bounds, whole-file rejection with line");
    return 0;
}
