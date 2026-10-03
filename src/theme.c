#include "theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void theme_default(XTheme *t)
{
    static const XTheme dark={
        {0x101820,0xe66c75,0x98c379,0xe5c07b,0x61afef,0xc678dd,0x56b6c2,0xabb2bf,
         0x5c6370,0xff8c95,0xb8e399,0xffe09b,0x81cfff,0xe698fd,0x76d6e2,0xf4f6fa},
        0xabb2bf,0x101820,0xf4f6fa,0xffffff,0x355879};
    *t=dark;
}
static uint32_t indexed_rgb(const XTheme *t,unsigned index)
{
    static const unsigned levels[]={0,95,135,175,215,255};
    if(index<16) return t->ansi[index];
    if(index<232) {
        unsigned cube=index-16;
        return (levels[cube/36]<<16) | (levels[(cube/6)%6]<<8) |
               levels[cube%6];
    }
    return (8+10*(index-232))*0x010101u;
}
/* Workbench pens in the theme's colours: 0 background, 1 text, 2 the
   opposite of the background (white on a light theme, black on a dark one),
   3 the accent. AROS One's prompt is pen 1 on 2, then pen 2 on 3. */
static uint32_t pen_rgb(const XTheme *t,unsigned pen)
{
    uint32_t b=t->background;
    int dark=((b>>16&255)*299+(b>>8&255)*587+(b&255)*114)/1000<128;
    switch(pen&7) {
    case 0: return t->background;
    case 1: return t->foreground;
    case 2: return dark?0x000000:0xffffff;
    case 3: return dark?t->ansi[12]:t->ansi[4];
    case 4: return t->ansi[8];
    case 5: return t->ansi[3];
    case 6: return t->ansi[14];
    default: return t->ansi[11];
    }
}
static uint32_t base_fg(const XTheme *t,const XTermCell *c)
{
    unsigned i=c->fg;
    if(c->colour_flags&XCOLOUR_DEFAULT_FG) return t->foreground;
    if(c->colour_flags&XCOLOUR_RGB_FG) return c->fg_rgb;
    if((c->colour_flags&XCOLOUR_PEN_FG) && i<8) return pen_rgb(t,i);
    if(c->bold && i<8) i+=8;
    return indexed_rgb(t,i);
}
static uint32_t base_bg(const XTheme *t,const XTermCell *c)
{
    if(c->colour_flags&XCOLOUR_DEFAULT_BG) return t->background;
    if(c->colour_flags&XCOLOUR_RGB_BG) return c->bg_rgb;
    if((c->colour_flags&XCOLOUR_PEN_BG) && c->bg<8) return pen_rgb(t,c->bg);
    return indexed_rgb(t,c->bg);
}
uint32_t theme_fg(const XTheme *t,const XTermCell *c)
{
    if(c->colour_flags&XCOLOUR_SELECTED) return t->selection_fg;
    return c->colour_flags&XCOLOUR_INVERSE ? base_bg(t,c) : base_fg(t,c);
}
uint32_t theme_bg(const XTheme *t,const XTermCell *c)
{
    if(c->colour_flags&XCOLOUR_SELECTED) return t->selection_bg;
    return c->colour_flags&XCOLOUR_INVERSE ? base_fg(t,c) : base_bg(t,c);
}
int theme_parse(XTheme *t,const char *text)
{
    XTheme next=*t;
    unsigned long seen=0;
    size_t total=0;
    while(*text) {
        char line[96], *eq, *end;
        size_t n=0;
        unsigned index=21, i;
        uint32_t rgb=0, *dest=NULL;
        static const char *keys[]={"foreground","background","cursor","selection_fg","selection_bg"};
        while(*text && *text!='\n') {
            if(n>=sizeof line-1 || ++total>4096) return -1;
            line[n++]=*text++;
        }
        if(*text=='\n') { ++text; if(++total>4096) return -1; }
        if(n && line[n-1]=='\r') --n;
        line[n]=0;
        if(!n || line[0]=='#') continue;
        eq=strchr(line,'='); if(!eq) return -1;
        *eq++=0;
        for(i=0;i<5;++i) if(!strcmp(line,keys[i])) index=16+i;
        if(!strncmp(line,"ansi",4)) {
            unsigned long value;
            if(line[4]<'0' || line[4]>'9') return -1;
            value=strtoul(line+4,&end,10);
            if(*end || value>15) return -1;
            index=(unsigned)value;
        }
        if(index>=21 || (seen&(1UL<<index))) return -1;
        seen|=1UL<<index;
        if(strlen(eq)!=7 || *eq++!='#') return -1;
        for(i=0;i<6;++i) {
            unsigned char c=(unsigned char)eq[i]; unsigned digit;
            if(c>='0' && c<='9') digit=c-'0';
            else if(c>='a' && c<='f') digit=c-'a'+10;
            else if(c>='A' && c<='F') digit=c-'A'+10;
            else return -1;
            rgb=(rgb<<4)|digit;
        }
        if(index<16) dest=&next.ansi[index];
        else switch(index) {
            case 16: dest=&next.foreground; break;
            case 17: dest=&next.background; break;
            case 18: dest=&next.cursor; break;
            case 19: dest=&next.selection_fg; break;
            case 20: dest=&next.selection_bg; break;
        }
        *dest=rgb;
    }
    *t=next; return 0;
}
int theme_load(XTheme *t,const char *path)
{
    char text[4097]; size_t n; int error;
    FILE *f=fopen(path,"rb");
    if(!f) return -1;
    n=fread(text,1,sizeof text,f); error=ferror(f); fclose(f);
    if(error || n>4096 || memchr(text,0,n)) return -1;
    text[n]=0; return theme_parse(t,text);
}
