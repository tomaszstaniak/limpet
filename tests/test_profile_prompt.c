/* Catch inherited Shell colours and a missing trailing SGR reset.
 * Decode only the example's quoted Prompt and *E escapes, not AmigaDOS.
 * Native Shell execution remains a separate integration check. */
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen screen;
static XTermCell cells[160];
int main(void)
{
    char line[512], prompt[512];
    size_t n=0, i;
    int found=0;
    FILE *f=fopen("examples/Shell-Startup","r");
    assert(f);
    while(fgets(line,sizeof line,f)) {
        char *p;
        if(strncmp(line,"Prompt \"",8)) continue;
        found++;
        for(p=line+8;*p && *p!='"';p++) {
            if(*p=='*') { ++p; assert(*p=='E'); prompt[n++]=27; }
            else prompt[n++]=*p;
        }
        assert(*p=='"');
    }
    fclose(f); assert(found==1 && n);
    assert(!screen_init(&screen,cells,160,2,80));
    screen_write(&screen,(const unsigned char *)"\033[1;31;43m",10);
    screen_write(&screen,(const unsigned char *)prompt,n);
    for(i=0;i<screen.cursor_column;i++) {
        assert(cells[i].bg==0);
        assert(cells[i].fg==7 || cells[i].fg==6);
    }
    assert(screen.style.fg==7 && screen.style.bg==0 && !screen.style.bold);
    screen_write(&screen,(const unsigned char *)"Echo OK",7);
    assert(cells[screen.cursor_column-1].fg==7);
    /* ANSI red remains red; no native-pen remapping in the terminal. */
    screen_write(&screen,(const unsigned char *)"\033[31mR",6);
    assert(cells[screen.cursor_column-1].fg==1);
    puts("profile prompt PASS: clean background, readable foreground, reset, ANSI preserved");
    return 0;
}
