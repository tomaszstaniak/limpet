#include "../src/theme_list.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    static const XThemeEntry bundled[]={{"light.theme",0},{"dark.theme",0},{"magicwb.theme",0},{"dark.theme.info",0}};
    static const XThemeEntry user[]={{"Dracula",0},{"nord.conf",0},{"Dark.theme",0},{".hidden",0},{"old.bak",0},
                                     {"Solarized Dark.itermcolors",0},{"x.new",0},{"",0}};
    XThemeEntry out[16];
    char label[40];
    unsigned n=theme_list_merge(out,16,bundled,4,user,8);
    assert(n==6);
    /* Sorted without case; the user's Dark.theme hides the program's. */
    assert(!strcmp(out[0].name,"Dark.theme") && out[0].user==1);
    assert(!strcmp(out[1].name,"Dracula") && out[1].user==1);
    assert(!strcmp(out[2].name,"light.theme") && out[2].user==0);
    assert(!strcmp(out[3].name,"magicwb.theme") && out[3].user==0);
    assert(!strcmp(out[4].name,"nord.conf") && out[4].user==1);
    assert(!strcmp(out[5].name,"Solarized Dark.itermcolors"));
    /* Capacity is respected. */
    assert(theme_list_merge(out,2,bundled,4,user,8)<=2);
    assert(theme_list_merge(out,16,NULL,0,NULL,0)==0);
    n=theme_list_merge(out,16,bundled,4,user,8);
    theme_list_label(label,sizeof label,"Solarized Dark.itermcolors"); assert(!strcmp(label,"Solarized Dark"));
    theme_list_label(label,sizeof label,"nord.conf"); assert(!strcmp(label,"nord"));
    theme_list_label(label,sizeof label,"magicwb.THEME"); assert(!strcmp(label,"magicwb"));
    theme_list_label(label,sizeof label,"Dracula"); assert(!strcmp(label,"Dracula"));
    theme_list_label(label,sizeof label,".theme"); assert(!strcmp(label,".theme"));
    /* What a theme= value selects. */
    assert(theme_list_find(out,n,"Dracula")==1);
    assert(theme_list_find(out,n,"dracula")==1);
    assert(theme_list_find(out,n,"magicwb")==3);
    assert(theme_list_find(out,n,"magicwb.theme")==3);
    assert(theme_list_find(out,n,"nord")==4);
    assert(theme_list_find(out,n,"Solarized Dark")==5);
    assert(theme_list_find(out,n,"Work:Themes/Dracula")==-1);
    assert(theme_list_find(out,n,"none")==-1);
    puts("theme list PASS: merged and sorted, the user's file first, non-themes left out, labels, values found");
    return 0;
}
