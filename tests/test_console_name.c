#include "../src/console_name.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XConsoleName c;
int main(void)
{
    /* Bare name: nothing given. */
    assert(!console_name_parse(&c,"XTERM:"));
    assert(c.left==-1 && c.top==-1 && c.width==-1 && c.height==-1 && !c.title[0] && !c.options && c.close_gadget==-1);
    /* NewShell's default window. */
    assert(!console_name_parse(&c,"CON:10/50/640/480/AROS-Shell/CLOSE"));
    assert(c.left==10 && c.top==50 && c.width==640 && c.height==480);
    assert(!strcmp(c.title,"AROS-Shell") && c.options==XCON_CLOSE && c.close_gadget==1);
    /* CLI.c's: an empty field keeps the default. */
    assert(!console_name_parse(&c,"XTERM:0/50//130/AROS-Shell/CLOSE"));
    assert(c.left==0 && c.top==50 && c.width==-1 && c.height==130);
    /* The boot console: no geometry, several options. */
    assert(!console_name_parse(&c,"CON:////AROS/AUTO/CLOSE/SMART/BOOT"));
    assert(c.width==-1 && !strcmp(c.title,"AROS"));
    assert(c.options==(XCON_AUTO|XCON_CLOSE|XCON_SMART|XCON_BOOT));
    /* Without the device part. */
    assert(!console_name_parse(&c,"1/2/3/4/T"));
    assert(c.left==1 && c.height==4 && !strcmp(c.title,"T"));
    /* Bad geometry: any non-digit in fields 1-4. */
    assert(console_name_parse(&c,"XTERM:a/0/100/100")==-1);
    assert(console_name_parse(&c,"XTERM:0/0/-1/100")==-1);
    assert(console_name_parse(&c,"XTERM:0/0/64 0/100")==-1);
    /* Title with spaces and anything but '/'. */
    assert(!console_name_parse(&c,"XTERM:0/0/640/200/My Tool: log (1)/CLOSE"));
    assert(!strcmp(c.title,"My Tool: log (1)"));
    /* Prefixes, in CON:'s order, without case. */
    assert(!console_name_parse(&c,"X:////t/c")); assert(c.options==XCON_CLOSE);
    assert(!console_name_parse(&c,"X:////t/n")); assert(c.options==XCON_NOCLOSE && c.close_gadget==0);
    assert(!console_name_parse(&c,"X:////t/s")); assert(c.options==XCON_SIMPLE);
    assert(!console_name_parse(&c,"X:////t/w")); assert(c.options==XCON_WAIT);
    assert(!console_name_parse(&c,"X:////t/NOS")); assert(c.options==XCON_NOSIZE);
    assert(!console_name_parse(&c,"X:////t/nodrag/nodepth/noborder/inactive"));
    assert(c.options==(XCON_NODRAG|XCON_NODEPTH|XCON_NOBORDER|XCON_INACTIVE));
    assert(!console_name_parse(&c,"X:////t/BACKDROP/ALT30/30/200/200"));
    /* ALT30 is not a prefix of ALT: ignored, as in CON:; 30,200 are options too. */
    assert(c.options==XCON_BACKDROP);
    /* The later of CLOSE and NOCLOSE wins. */
    assert(!console_name_parse(&c,"X:////t/NOCLOSE/CLOSE")); assert(c.close_gadget==1);
    /* SCREEN and WINDOW take the rest of the field. */
    assert(!console_name_parse(&c,"X:0/0/640/200/t/SCREENWorkbench"));
    assert(c.options==XCON_SCREEN && !strcmp(c.screen,"Workbench"));
    assert(!console_name_parse(&c,"X:////t/WINDOW0x1F00"));
    assert(c.options==XCON_WINDOW && c.window==0x1f00);
    assert(!console_name_parse(&c,"X:////t/window1234")); assert(c.window==1234);
    /* Unknown options are ignored; a longer word is not a prefix. */
    assert(!console_name_parse(&c,"X:////t/CLOSEX/FOO/"));
    assert(!c.options);
    /* Limits. */
    { char name[300]="X:0/0/1/1/"; memset(name+10,'a',200); name[210]=0;
      assert(console_name_parse(&c,name)==-1); }
    puts("console name PASS: CON: fields, defaults, prefixes, SCREEN/WINDOW values, bad names");
    return 0;
}
