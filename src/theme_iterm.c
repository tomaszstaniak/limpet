#include "theme_iterm.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Display P3 to sRGB: both use the sRGB transfer curve; the primaries
   differ. Colours outside sRGB are clamped. */
static double linear(double v) { return v<=0.04045?v/12.92:pow((v+0.055)/1.055,2.4); }
static double encoded(double v)
{
    if(v<=0) return 0;
    if(v>=1) return 1;
    return v<=0.0031308?v*12.92:1.055*pow(v,1/2.4)-0.055;
}
static unsigned byte_of(double v)
{
    if(!(v>0)) return 0;          /* also a NaN */
    if(v>=1) return 255;
    return (unsigned)(v*255+0.5);
}

typedef struct {
    double component[3];          /* red, green, blue */
    unsigned given;               /* bit per component */
    int p3;
} Colour;

static uint32_t rgb_of(const Colour *c)
{
    double r=c->component[0],g=c->component[1],b=c->component[2];
    if(c->p3) {
        double lr=linear(r<0?0:r>1?1:r),lg=linear(g<0?0:g>1?1:g),lb=linear(b<0?0:b>1?1:b);
        r=encoded( 1.2249401*lr-0.2249404*lg+0.0000000*lb);
        g=encoded(-0.0420569*lr+1.0420571*lg+0.0000000*lb);
        b=encoded(-0.0196376*lr-0.0786361*lg+1.0982735*lb);
    }
    return byte_of(r)<<16|byte_of(g)<<8|byte_of(b);
}

static int is(const char *text,const char *word) { return !strcmp(text,word); }

/* The role an outer key names: 0..15 a palette entry, an XROLE_* bit, -2
   a colour Limpet has no use for but counts, -1 anything else. */
static long role_of(const char *key)
{
    if(!strncmp(key,"Ansi ",5)) {
        unsigned n=0;
        const char *p=key+5;
        if(*p<'0' || *p>'9') return -1;
        while(*p>='0' && *p<='9' && n<100) n=n*10+(unsigned)(*p++-'0');
        return is(p," Color") && n<16?(long)n:-1;
    }
    if(is(key,"Foreground Color")) return XROLE_FG;
    if(is(key,"Background Color")) return XROLE_BG;
    if(is(key,"Cursor Color")) return XROLE_CURSOR;
    if(is(key,"Selection Color")) return XROLE_SEL_BG;
    if(is(key,"Selected Text Color")) return XROLE_SEL_FG;
    if(is(key,"Bold Color")) return -2;
    return -1;
}

/* The text up to the next '<', trimmed of nothing: plist values are bare. */
static const char *text_to(const char *p,const char *end,char *out,size_t size)
{
    size_t n=0;
    while(p<end && *p!='<') { if(n<size-1) out[n++]=*p; ++p; }
    out[n]=0;
    return p;
}

int theme_iterm_parse(XTheme *t,unsigned long *seen,const char *p,size_t length,XThemeReport *r)
{
    const char *end=p+length;
    char outer[48]="",inner[32]="",value[32];
    Colour colour;
    int depth=0;
    memset(&colour,0,sizeof colour);
    while(p<end) {
        const char *tag,*close;
        size_t n;
        while(p<end && *p!='<') ++p;
        if(p==end) break;
        tag=++p;
        while(p<end && *p!='>') ++p;
        if(p==end) break;                       /* cut off inside a tag */
        close=p++; n=(size_t)(close-tag);
        if(n==3 && !memcmp(tag,"key",3)) {
            if(depth==1) p=text_to(p,end,outer,sizeof outer);
            else if(depth==2) p=text_to(p,end,inner,sizeof inner);
        } else if(n==4 && !memcmp(tag,"dict",4)) {
            if(++depth==2) { memset(&colour,0,sizeof colour); inner[0]=0; }
        } else if(n==5 && !memcmp(tag,"/dict",5)) {
            if(depth==2 && colour.given==7) {
                long role=role_of(outer);
                uint32_t rgb=rgb_of(&colour);
                if(role==-2) ++r->ignored;
                else if(role>=0 && role<16) { t->ansi[role]=rgb; *seen|=1UL<<role; }
                else if(role==XROLE_FG) { t->foreground=rgb; *seen|=XROLE_FG; }
                else if(role==XROLE_BG) { t->background=rgb; *seen|=XROLE_BG; }
                else if(role==XROLE_CURSOR) { t->cursor=rgb; *seen|=XROLE_CURSOR; }
                else if(role==XROLE_SEL_BG) { t->selection_bg=rgb; *seen|=XROLE_SEL_BG; }
                else if(role==XROLE_SEL_FG) { t->selection_fg=rgb; *seen|=XROLE_SEL_FG; }
            }
            if(depth>0) --depth;
            if(depth==1) outer[0]=0;
        } else if(depth==2 && ((n==4 && !memcmp(tag,"real",4)) || (n==7 && !memcmp(tag,"integer",7)))) {
            int which=is(inner,"Red Component")?0:is(inner,"Green Component")?1:is(inner,"Blue Component")?2:-1;
            p=text_to(p,end,value,sizeof value);
            if(which>=0) {
                char *stop;
                double v=strtod(value,&stop);
                if(stop!=value) { colour.component[which]=v; colour.given|=1u<<which; }
            }
            inner[0]=0;
        } else if(depth==2 && n==6 && !memcmp(tag,"string",6)) {
            p=text_to(p,end,value,sizeof value);
            if(is(inner,"Color Space")) colour.p3=is(value,"P3");
            inner[0]=0;
        }
    }
    return 0;
}
