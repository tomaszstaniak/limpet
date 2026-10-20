/* Bounded native OSC8 -> cell lookup -> AmigaDOS Lock probe.
 * Retains a named fixture so the renderer click path can be tested separately. */
#include "../src/screen_model.h"
#include "../src/hyperlink.h"
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static XTermScreen screen;
static XTermCell cells[120];
static XHyperlinks links;
int main(void)
{
    const char content[]="eza OSC8 local file fixture\n";
    const char *name="RAM:eza link fixture.txt";
    const char *osc="\033]8;;file:///RAM:eza%20link%20fixture.txt\033\\Document\033]8;;\a";
    char path[2048],canonical[2048];BPTR file,lock;XHyperlinkHit hit;size_t i;int result=10;
    file=Open((CONST_STRPTR)name,MODE_NEWFILE);if(!file)return 20;
    if(Write(file,content,sizeof content-1)!=(LONG)(sizeof content-1)){Close(file);return 20;}Close(file);
    if(screen_init(&screen,cells,120,3,40))return 20;
    hyperlinks_init(&links);screen_attach_links(&screen,&links);
    for(i=0;i<strlen(osc);++i)screen_write(&screen,(const unsigned char *)osc+i,1);
    if(hyperlink_at(&screen,0,2,&hit) && hyperlink_file_path(hit.url,path,sizeof path) && !strcmp(path,name)) {
        lock=Lock((CONST_STRPTR)path,ACCESS_READ);
        if(lock) {if(NameFromLock(lock,(STRPTR)canonical,sizeof canonical)){printf("uri=%s target=%s resolved=%s lookup=PASS\n",hit.url,path,canonical);result=0;}UnLock(lock);}
    }
    printf("fixture_retained=%s result=%d\n",name,result);return result;
}
