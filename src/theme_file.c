#include "theme_file.h"
#include "theme_iterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int blank(char c) { return c==' ' || c=='\t'; }
static int starts(const char *p,const char *end,const char *word)
{
    size_t n=strlen(word);
    return (size_t)(end-p)>=n && !memcmp(p,word,n);
}
static int is(const char *p,size_t n,const char *word)
{ return strlen(word)==n && !memcmp(p,word,n); }
static int hex(char c)
{
    if(c>='0' && c<='9') return c-'0';
    if(c>='a' && c<='f') return c-'a'+10;
    if(c>='A' && c<='F') return c-'A'+10;
    return -1;
}
/* #rrggbb, rrggbb or #rgb, the whole of the n bytes. */
static int colour(const char *p,size_t n,uint32_t *out)
{
    uint32_t value=0;
    size_t i;
    int short_form=0;
    if(n && *p=='#') { ++p; --n; short_form=n==3; }
    if(n!=6 && !short_form) return 0;
    for(i=0;i<n;++i) {
        int d=hex(p[i]);
        if(d<0) return 0;
        value=short_form?(value<<8)|(uint32_t)(d*17):(value<<4)|(uint32_t)d;
    }
    *out=value;
    return 1;
}
/* A palette index: decimal, or with 0x, 0o or 0b as Ghostty takes it. */
static int number(const char *p,size_t n,unsigned *out)
{
    unsigned base=10,value=0;
    size_t i=0;
    if(n>2 && p[0]=='0' && (p[1]=='x' || p[1]=='o' || p[1]=='b')) {
        base=p[1]=='x'?16:p[1]=='o'?8:2; i=2;
    }
    if(i==n) return 0;
    for(;i<n;++i) {
        int d=hex(p[i]);
        if(d<0 || (unsigned)d>=base || value>4096) return 0;
        value=value*base+(unsigned)d;
    }
    *out=value;
    return 1;
}

/* Keys only Limpet's own format has: a file with one of them is that
   format, and is read strictly. */
static int native_key(const char *k,size_t n)
{
    if(is(k,n,"selection_fg") || is(k,n,"selection_bg")) return 1;
    if(n>4 && !memcmp(k,"ansi",4) && k[4]>='0' && k[4]<='9') return 1;
    if(n==4 && !memcmp(k,"pen",3) && k[3]>='0' && k[3]<='9') return 1;
    return 0;
}

enum { ROLE_NONE, ROLE_PALETTE, ROLE_FG, ROLE_BG, ROLE_CURSOR, ROLE_SEL_FG, ROLE_SEL_BG, ROLE_SILENT };
static int role_of(const char *k,size_t n)
{
    if(is(k,n,"foreground")) return ROLE_FG;
    if(is(k,n,"background")) return ROLE_BG;
    if(is(k,n,"cursor") || is(k,n,"cursor-color")) return ROLE_CURSOR;
    if(is(k,n,"selection_foreground") || is(k,n,"selection-foreground")) return ROLE_SEL_FG;
    if(is(k,n,"selection_background") || is(k,n,"selection-background")) return ROLE_SEL_BG;
    /* The colour of text under a block cursor: Limpet's cursor is a line. */
    if(is(k,n,"cursor_text_color") || is(k,n,"cursor-text")) return ROLE_SILENT;
    return ROLE_NONE;
}

static int fail(XThemeReport *r,int error,int line)
{ r->error=error; r->line=line; return -1; }

static int lines(XTheme *t,const char *p,const char *end,XThemeReport *r)
{
    XTheme next;
    unsigned long seen=0;
    int line=0;
    theme_default(&next);
    next.bold_bright=t->bold_bright;
    while(p<end) {
        const char *stop=p,*k,*ke,*v,*ve;
        int role;
        unsigned index=0;
        uint32_t rgb;
        ++line;
        while(stop<end && *stop!='\n') ++stop;
        ve=stop;
        if(ve>p && ve[-1]=='\r') --ve;
        k=p; p=stop<end?stop+1:end;
        while(k<ve && blank(*k)) ++k;
        while(ve>k && blank(ve[-1])) --ve;
        if(k==ve || *k=='#') continue;
        /* A sectioned file (Alacritty, foot, WezTerm) is another format. */
        if(*k=='[' && !seen) return fail(r,XTHEME_NOT_A_THEME_FORMAT,0);
        ke=k;
        while(ke<ve && !blank(*ke) && *ke!='=') ++ke;
        v=ke;
        while(v<ve && blank(*v)) ++v;
        if(v<ve && *v=='=') { ++v; while(v<ve && blank(*v)) ++v; }
        if(ve-v>=2 && *v=='"' && ve[-1]=='"') { ++v; --ve; }
        role=role_of(k,(size_t)(ke-k));
        if(role==ROLE_SILENT) continue;
        if(is(k,(size_t)(ke-k),"palette")) {
            /* Ghostty: palette = N=COLOUR */
            const char *eq=v;
            while(eq<ve && *eq!='=') ++eq;
            if(eq==ve) return fail(r,XTHEME_BAD_COLOUR,line);
            {
                const char *ne=eq;
                while(ne>v && blank(ne[-1])) --ne;
                if(!number(v,(size_t)(ne-v),&index) || index>255) return fail(r,XTHEME_BAD_COLOUR,line);
            }
            v=eq+1;
            while(v<ve && blank(*v)) ++v;
            if(ve-v>=2 && *v=='"' && ve[-1]=='"') { ++v; --ve; }
            role=ROLE_PALETTE;
        } else if(role==ROLE_NONE && ke-k>5 && !memcmp(k,"color",5) && k[5]>='0' && k[5]<='9') {
            /* Kitty: colorN COLOUR */
            if(!number(k+5,(size_t)(ke-k-5),&index) || index>255) return fail(r,XTHEME_BAD_COLOUR,line);
            role=ROLE_PALETTE;
        }
        if(role==ROLE_NONE) { ++r->ignored; continue; }
        /* "none" (Kitty) and the cell's own colour (Ghostty): Limpet has
           one selection and one cursor colour, so the role is derived. */
        if(role>=ROLE_CURSOR && (is(v,(size_t)(ve-v),"none") || is(v,(size_t)(ve-v),"cell-foreground") ||
                                 is(v,(size_t)(ve-v),"cell-background"))) continue;
        if(!colour(v,(size_t)(ve-v),&rgb)) return fail(r,XTHEME_BAD_COLOUR,line);
        if(role==ROLE_PALETTE) {
            if(index<16) { next.ansi[index]=rgb; seen|=1UL<<index; }
            else ++r->ignored;                   /* palette 16..255: fixed in Limpet */
        }
        else if(role==ROLE_FG) { next.foreground=rgb; seen|=XROLE_FG; }
        else if(role==ROLE_BG) { next.background=rgb; seen|=XROLE_BG; }
        else if(role==ROLE_CURSOR) { next.cursor=rgb; seen|=XROLE_CURSOR; }
        else if(role==ROLE_SEL_FG) { next.selection_fg=rgb; seen|=XROLE_SEL_FG; }
        else if(role==ROLE_SEL_BG) { next.selection_bg=rgb; seen|=XROLE_SEL_BG; }
    }
    if(theme_complete(&next,seen)) return fail(r,XTHEME_NO_COLOURS,0);
    r->format=XTHEME_FORMAT_LINES;
    *t=next;
    return 0;
}

static int has_native_key(const char *p,const char *end)
{
    while(p<end) {
        const char *stop=p,*k,*ke;
        while(stop<end && *stop!='\n') ++stop;
        k=p; p=stop<end?stop+1:end;
        while(k<stop && blank(*k)) ++k;
        ke=k;
        while(ke<stop && !blank(*ke) && *ke!='=' && *ke!='\r') ++ke;
        if(native_key(k,(size_t)(ke-k))) return 1;
    }
    return 0;
}

int theme_file_parse(XTheme *t,const char *bytes,size_t length,XThemeReport *r)
{
    const char *p=bytes,*end=bytes+length,*first;
    memset(r,0,sizeof *r);
    if(length>XTHEME_FILE_MAX) return fail(r,XTHEME_TOO_LARGE,0);
    if(starts(p,end,"bplist00")) return fail(r,XTHEME_BINARY_PLIST,0);
    if(starts(p,end,"\xff\xfe") || starts(p,end,"\xfe\xff") || memchr(bytes,0,length))
        return fail(r,XTHEME_NOT_TEXT,0);
    if(starts(p,end,"\xef\xbb\xbf")) p+=3;
    first=p;
    while(first<end && (blank(*first) || *first=='\n' || *first=='\r')) ++first;
    if(starts(first,end,"<?xml") || starts(first,end,"<!DOCTYPE plist") || starts(first,end,"<plist")) {
        XTheme next;
        unsigned long seen=0;
        theme_default(&next);
        next.bold_bright=t->bold_bright;
        theme_iterm_parse(&next,&seen,first,(size_t)(end-first),r);
        if(theme_complete(&next,seen)) return fail(r,XTHEME_NO_COLOURS,0);
        r->format=XTHEME_FORMAT_ITERM;
        *t=next;
        return 0;
    }
    if(first<end && *first=='{') return fail(r,XTHEME_NOT_A_THEME_FORMAT,0);
    if(has_native_key(p,end)) {
        /* Limpet's own format, as theme_parse always read it: strict, at
           most 4096 bytes, over the built-in dark theme. */
        char text[4097];
        XTheme next;
        size_t n=(size_t)(end-p);
        if(n>4096) return fail(r,XTHEME_BAD_NATIVE,0);
        memcpy(text,p,n); text[n]=0;
        theme_default(&next);
        next.bold_bright=t->bold_bright;
        if(theme_parse(&next,text)) return fail(r,XTHEME_BAD_NATIVE,0);
        r->format=XTHEME_FORMAT_NATIVE;
        *t=next;
        return 0;
    }
    return lines(t,p,end,r);
}

int theme_file_load(XTheme *t,const char *path,XThemeReport *r)
{
    char *bytes;
    size_t n;
    int error,result;
    FILE *f=fopen(path,"rb");
    memset(r,0,sizeof *r);
    if(!f) return fail(r,XTHEME_CANNOT_READ,0);
    bytes=malloc(XTHEME_FILE_MAX+1);
    if(!bytes) { fclose(f); return fail(r,XTHEME_CANNOT_READ,0); }
    n=fread(bytes,1,XTHEME_FILE_MAX+1,f); error=ferror(f); fclose(f);
    if(error) result=fail(r,XTHEME_CANNOT_READ,0);
    else result=theme_file_parse(t,bytes,n,r);
    free(bytes);
    return result;
}

void theme_file_message(char *out,size_t size,const char *name,const XThemeReport *r,const char *instead)
{
    const char *what=NULL;
    if(size) *out=0;
    switch(r->error) {
    case XTHEME_OK:
        if(r->ignored) snprintf(out,size,"Theme %.60s: %u setting%s ignored",name,r->ignored,r->ignored==1?"":"s");
        return;
    case XTHEME_BAD_COLOUR:
        snprintf(out,size,"Settings: theme %.60s line %d: not a colour; %s",name,r->line,instead);
        return;
    case XTHEME_CANNOT_READ: what="cannot be read"; break;
    case XTHEME_TOO_LARGE: what="is over 64 KB"; break;
    case XTHEME_NOT_TEXT: what="is not a text file"; break;
    case XTHEME_BINARY_PLIST: what="is a binary property list, export it as XML"; break;
    case XTHEME_NOT_A_THEME_FORMAT: what="is not a Ghostty, Kitty or iTerm2 colour file"; break;
    case XTHEME_NO_COLOURS: what="has no colours"; break;
    default: what="is not a valid Limpet theme"; break;
    }
    snprintf(out,size,"Settings: theme %.60s %s; %s",name,what,instead);
}

int theme_locate(char *out,size_t size,const char *value,int (*exists)(const char *path))
{
    static const char *const drawers[]={XTHEME_USER_DRAWER "/",XTHEME_PROGRAM_DRAWER "/"};
    static const char *const endings[]={"",".theme",".conf",".itermcolors"};
    unsigned d,e;
    if(!*value) return -1;
    if(strchr(value,':') || strchr(value,'/')) {
        if(strlen(value)>=size) return -1;
        strcpy(out,value);
        return 0;
    }
    for(d=0;d<2;++d) for(e=0;e<4;++e) {
        if(strlen(drawers[d])+strlen(value)+strlen(endings[e])>=size) continue;
        snprintf(out,size,"%s%s%s",drawers[d],value,endings[e]);
        if(exists(out)) return 0;
    }
    return -1;
}
