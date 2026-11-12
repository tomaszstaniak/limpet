#include "theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void theme_default(XTheme *t)
{
    static const XTheme dark={
        {0x101820,0xe66c75,0x98c379,0xe5c07b,0x61afef,0xc678dd,0x56b6c2,0xabb2bf,
         0x5c6370,0xff8c95,0xb8e399,0xffe09b,0x81cfff,0xe698fd,0x76d6e2,0xf4f6fa},
        0xabb2bf,0x101820,0xf4f6fa,0xffffff,0x355879,
        {0,0,0,0,0,0,0,0},0,1};
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
unsigned theme_luma(uint32_t rgb)
{ return ((rgb>>16&255)*299+(rgb>>8&255)*587+(rgb&255)*114)/1000; }
static unsigned apart(uint32_t a,uint32_t b)
{ int d=(int)theme_luma(a)-(int)theme_luma(b); return (unsigned)(d<0?-d:d); }
/* Workbench pens in the theme's colours: 0 background, 1 text, 2 the
   opposite of the background (white on a light theme, black on a dark one),
   3 the accent. AROS One's prompt is pen 1 on 2, then pen 2 on 3. A theme
   may give pens 2..7 itself. The rest follow the palette, which a scheme
   made for another terminal fills as it likes: the accent must then stand
   out from pen 2 and not be the text colour, and a pen used as text must
   not sink into the background. */
uint32_t theme_pen(const XTheme *t,unsigned pen)
{
    int dark=theme_luma(t->background)<128;
    uint32_t two=(t->pen_set&4)?t->pen[2]:dark?0x000000:0xffffff, c;
    pen&=7;
    if(pen==0) return t->background;
    if(pen==1) return t->foreground;
    if(t->pen_set&(1u<<pen)) return t->pen[pen];
    switch(pen) {
    case 2: return two;
    case 3: {
        uint32_t first=dark?t->ansi[12]:t->ansi[4], second=dark?t->ansi[4]:t->ansi[12];
        if(first!=t->foreground && apart(first,two)>=48) return first;
        if(second!=t->foreground && apart(second,two)>=48) return second;
        return t->selection_bg;
    }
    case 4: c=t->ansi[8]; break;
    case 5: c=t->ansi[3]; break;
    case 6: c=t->ansi[14]; break;
    default: c=t->ansi[11]; break;
    }
    return apart(c,t->background)<40?t->foreground:c;
}
int theme_complete(XTheme *t,unsigned long seen)
{
    /* themes/light.theme's palette, for a light background without one. */
    static const uint32_t light[16]={
        0x101820,0xa82030,0x286020,0x785000,0x1858a0,0x803080,0x006070,0xb0b0b0,
        0x606060,0xc03040,0x307028,0x886000,0x2868b0,0x904090,0x007080,0xffffff};
    unsigned i,palette=0;
    for(i=0;i<16;++i) if(seen&(1UL<<i)) ++palette;
    if(!((seen&XROLE_FG) && (seen&XROLE_BG)) && palette<8) return -1;
    if(!(seen&0xffffUL)) {
        /* No palette at all: one that reads on this background. */
        if(theme_luma(t->background)>=128) memcpy(t->ansi,light,sizeof light);
        else { XTheme dark; theme_default(&dark); memcpy(t->ansi,dark.ansi,sizeof t->ansi); }
    } else for(i=0;i<8;++i) {
        if(!(seen&(1UL<<(8+i))) && (seen&(1UL<<i))) t->ansi[8+i]=t->ansi[i];
        else if(!(seen&(1UL<<i)) && (seen&(1UL<<(8+i)))) t->ansi[i]=t->ansi[8+i];
    }
    if(!(seen&XROLE_FG)) t->foreground=t->ansi[7];
    if(!(seen&XROLE_BG)) t->background=t->ansi[0];
    if(!(seen&XROLE_CURSOR)) t->cursor=t->foreground;
    if(!(seen&XROLE_SEL_BG)) {
        t->selection_bg=t->foreground;
        if(!(seen&XROLE_SEL_FG)) t->selection_fg=t->background;
    } else if(!(seen&XROLE_SEL_FG))
        t->selection_fg=apart(t->foreground,t->selection_bg)>=apart(t->background,t->selection_bg)?
            t->foreground:t->background;
    return 0;
}
static uint32_t base_fg(const XTheme *t,const XTermCell *c)
{
    unsigned i=c->fg;
    if(c->colour_flags&XCOLOUR_DEFAULT_FG) return t->foreground;
    if(c->colour_flags&XCOLOUR_RGB_FG) return c->fg_rgb;
    if((c->colour_flags&XCOLOUR_PEN_FG) && i<8) return theme_pen(t,i);
    if(t->bold_bright && c->bold && i<8) i+=8;
    return indexed_rgb(t,i);
}
static uint32_t base_bg(const XTheme *t,const XTermCell *c)
{
    if(c->colour_flags&XCOLOUR_DEFAULT_BG) return t->background;
    if(c->colour_flags&XCOLOUR_RGB_BG) return c->bg_rgb;
    if((c->colour_flags&XCOLOUR_PEN_BG) && c->bg<8) return theme_pen(t,c->bg);
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
        unsigned index=27, i;
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
        if(!strncmp(line,"pen",3)) {
            /* pen2..pen7; pens 0 and 1 are the grounds */
            if(line[3]<'2' || line[3]>'7' || line[4]) return -1;
            index=21+(unsigned)(line[3]-'2');
        }
        if(index>=27 || (seen&(1UL<<index))) return -1;
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
        if(index>=21) { dest=&next.pen[index-19]; next.pen_set|=1u<<(index-19); }
        else if(index<16) dest=&next.ansi[index];
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
