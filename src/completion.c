#include "completion.h"
#include "utf8proc.h"
#include <string.h>
static unsigned fold(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
static int compare(const char *a,const char *b)
{
    while(*a && fold((unsigned char)*a)==fold((unsigned char)*b)) { ++a; ++b; }
    return (int)fold((unsigned char)*a)-(int)fold((unsigned char)*b);
}
static int separator(char ch) { return ch==' ' || ch=='\t' || ch=='<' || ch=='>' || ch=='='; }
static int safe(const char *s)
{
    size_t n=strlen(s), at=0;
    while(at<n) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t used=utf8proc_iterate((const utf8proc_uint8_t *)s+at,n-at,&cp);
        if(used<=0 || cp<32 || (cp>=127 && cp<=159) || cp=='$' || cp=='`') return 0;
        at+=(size_t)used;
    }
    return 1;
}
int completion_prepare(XCompletion *c,const XLineEditor *e)
{
    size_t i, start=0, n=0, split=0;
    int quote=0, prior=0, token=0;
    char decoded[XLINE_CAPACITY];
    memset(c,0,sizeof *c);
    if(e->cursor>e->length || (e->selecting && e->anchor!=e->cursor)) return 0;
    for(i=0;i<e->cursor;i++) {
        char ch=e->text[i];
        if(!quote && separator(ch)) {
            if(token || ch=='<' || ch=='>' || ch=='=') prior=1;
            start=i+1; n=0; token=0; c->quoted=0; continue;
        }
        token=1;
        if(ch=='"') { quote=!quote; c->quoted=1; continue; }
        if(ch=='*') {
            if(i+1>=e->cursor) return 0;
            ch=e->text[++i];
            if(ch!='*' && ch!='"' && ch!=' ') return 0;
        }
        decoded[n++]=ch;
    }
    c->end=e->cursor;
    if(quote && e->text[c->end]=='"') ++c->end;
    if(c->end<e->length && !separator(e->text[c->end])) return 0;
    decoded[n]=0;
    if(!safe(decoded)) return 0;
    for(i=0;i<n;i++) if(decoded[i]=='/' || decoded[i]==':') split=i+1;
    memcpy(c->directory,decoded,split); c->directory[split]=0;
    strcpy(c->prefix,decoded+split);
    c->start=start; c->command=!prior && !split;
    strcpy(c->expected,e->text); c->cursor=e->cursor;
    return 1;
}
int completion_add(XCompletion *c,const char *s,int kind)
{
    char name[XLINE_CAPACITY];
    size_t n=strlen(s), p=strlen(c->prefix), i, pos;
    if(kind<0 || kind>2 || !n || n+2>sizeof name || !safe(s)) return 0;
    if(strchr(s,'/') || strchr(s,':') || n<p) return 0;
    for(i=0;i<p;i++) if(fold((unsigned char)s[i])!=fold((unsigned char)c->prefix[i])) return 0;
    memcpy(name,s,n);
    if(kind) name[n++]=kind==1?'/':':';
    name[n]=0;
    for(pos=0;pos<c->count && compare(c->names[pos],name)<0;pos++) {}
    if(pos<c->count && !compare(c->names[pos],name)) return 0;
    if(c->count==XCOMP_MAX) { c->limited=1; return 0; }
    memmove(c->names+pos+1,c->names+pos,(c->count-pos)*sizeof c->names[0]);
    strcpy(c->names[pos],name); ++c->count;
    return 1;
}
int completion_apply(XCompletion *c,XLineEditor *e,int reverse)
{
    char raw[XLINE_CAPACITY], out[XLINE_CAPACITY];
    size_t index,n,i,used=0,oldcursor=e->cursor,oldanchor=e->anchor;
    int oldselect=e->selecting, quote=c->quoted, directory;
    if(!c->count || !completion_current(c,e)) return 0;
    index=reverse?(c->applied?(c->next+c->count-2)%c->count:c->count-1):c->next%c->count;
    n=strlen(c->directory)+strlen(c->names[index]);
    if(n>=sizeof raw) return 0;
    strcpy(raw,c->directory); strcat(raw,c->names[index]);
    directory=n && (raw[n-1]=='/' || raw[n-1]==':');
    for(i=0;i<n;i++) if(strchr(" \t\"*<>;#?()[]|=",raw[i])) quote=1;
    if(quote) out[used++]='"';
    for(i=0;i<n;i++) {
        if(used+4>=sizeof out) return 0;
        if(raw[i]=='*' || raw[i]=='"') out[used++]='*';
        out[used++]=raw[i];
    }
    if(quote) out[used++]='"';
    if(!directory && !e->text[c->end]) out[used++]=' ';
    e->anchor=c->start; e->cursor=c->end; e->selecting=1;
    if(!line_editor_insert(e,out,used)) {
        e->cursor=oldcursor; e->anchor=oldanchor; e->selecting=oldselect; return 0;
    }
    c->end=c->start+used;
    if(directory && quote) --e->cursor;
    e->anchor=e->cursor;
    strcpy(c->expected,e->text); c->cursor=e->cursor;
    c->next=index+1; c->applied=1;
    return 1;
}
int completion_current(const XCompletion *c,const XLineEditor *e)
{ return !(e->selecting && e->anchor!=e->cursor) && c->cursor==e->cursor && !strcmp(c->expected,e->text); }
