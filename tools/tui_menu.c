#include "tui_menu.h"
#include <stdio.h>
const char tui_menu_enter[]="\033[?1049h\033[?25l\033[2J\033[H";
const char tui_menu_leave[]="\033[0m\033[?25h\033[?1049l";
int tui_menu_input(TuiMenu *s,unsigned char c)
{
    unsigned previous=s->selected;
    if(c=='q' || c=='Q' || c==3 || c=='\r' || c=='\n') return -1;
    if(c==27) { s->escape=1; return 0; }
    if(s->escape==1) { s->escape=(c=='[')?2:0; return 0; }
    if(s->escape==2) {
        s->escape=0;
        if(c=='A') s->selected=(s->selected+2)%3;
        else if(c=='B') s->selected=(s->selected+1)%3;
    }
    return previous!=s->selected;
}
size_t tui_menu_draw(const TuiMenu *s,char *out,size_t capacity)
{
    int n;
    if(!s || !out || !capacity || s->selected>2) return 0;
    n=snprintf(out,capacity,
        "\033[2;3H\033[1;36mAROS XTerm / native TUI\033[0m\033[K"
        "\033[4;3HUp / Down: select   Enter / q / Ctrl-C: return to Shell\033[K"
        "\033[6;3H%s  Native DOS session  \033[0m\033[K"
        "\033[7;3H%s  Unicode display     \033[0m\033[K"
        "\033[8;3H%s  Return to Shell     \033[0m\033[K"
        "\033[10;3HCJK: 中文 日本語 한글   Emoji: 😀 👨‍👩‍👧‍👦 🇵🇱   é\033[K"
        "\033[12;3HSelection: %u\033[K"
        "\033[14;3HSeparate DOS process; raw input; alternate screen.\033[K",
        s->selected==0?"\033[37;44m":"\033[0m",
        s->selected==1?"\033[37;44m":"\033[0m",
        s->selected==2?"\033[37;44m":"\033[0m",s->selected+1);
    return n>0 && (size_t)n<capacity?(size_t)n:0;
}
