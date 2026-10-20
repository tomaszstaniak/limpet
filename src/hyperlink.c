#include "hyperlink.h"
#include "screen_model.h"
#include <string.h>

static int hex(unsigned char c)
{ return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'); }
static int http_target(const char *p,int launch)
{
    size_t i,n=0,start,end;
    if(!p) return 0;
    while(n<XHYPERLINK_LIMIT && p[n]) ++n;
    if(n==XHYPERLINK_LIMIT) return 0;
    if(n>=7 && !memcmp(p,"http://",7)) start=7;
    else if(n>=8 && !memcmp(p,"https://",8)) start=8;
    else return 0;
    end=start;
    while(end<n && p[end]!='/' && p[end]!='?' && p[end]!='#') ++end;
    if(end==start) return 0;
    for(i=0;i<n;++i) {
        unsigned char c=(unsigned char)p[i];
        if(c<=32 || c>=127 || (i>=start && i<end && c=='@')) return 0;
        if(c=='%' && (i+2>=n || !hex((unsigned char)p[i+1]) || !hex((unsigned char)p[i+2]))) return 0;
        if(launch && strchr("\"'`$*\\<>|",c)) return 0;
    }
    return 1;
}
static unsigned unhex(unsigned char c)
{ return c<='9'?c-'0':(c|32)-'a'+10; }
int hyperlink_file_path(const char *url,char *out,size_t capacity)
{
    size_t i,n=0;int volume=0;
    if(!url || !out || !capacity || strncmp(url,"file:///",8)) return 0;
    for(i=8;url[i] && i<XHYPERLINK_LIMIT;++i) {
        unsigned char c=(unsigned char)url[i];
        if(c=='%') {
            if(i+2>=XHYPERLINK_LIMIT || !url[i+1] || !url[i+2] || !hex((unsigned char)url[i+1]) || !hex((unsigned char)url[i+2])) return 0;
            c=(unsigned char)(16*unhex((unsigned char)url[i+1])+unhex((unsigned char)url[i+2]));i+=2;
        } else if(c<=32 || c>=127 || c=='?' || c=='#' || c=='\\') return 0;
        if(c<32 || c==127 || n+1>=capacity) return 0;
        if(!volume && c=='/') return 0;
        if(c==':') { if(volume || !n) return 0;volume=1; }
        out[n++]=(char)c;
    }
    if(i>=XHYPERLINK_LIMIT || !volume) return 0;
    out[n]=0;return 1;
}
int hyperlink_open_safe(const char *p)
{ char path[XHYPERLINK_LIMIT];return http_target(p,1) || hyperlink_file_path(p,path,sizeof path); }

void hyperlinks_init(XHyperlinks *l)
{ if(l) { memset(l,0,sizeof *l); l->next_token=1; } }
void screen_attach_links(XTermScreen *s,XHyperlinks *l)
{ if(s) { s->links=l; s->active_link=0; } }
uint32_t hyperlink_register(XHyperlinks *l,const char *p)
{
    unsigned slot; uint32_t token;
    if(!l || !(http_target(p,0) || hyperlink_open_safe(p)) || !l->next_token || l->next_token==XHYPERLINK_INVALID)
        return XHYPERLINK_INVALID;
    token=l->next_token++; slot=l->next_slot;
    l->next_slot=(slot+1)%XHYPERLINK_ENTRIES;
    l->entries[slot].token=token;
    strcpy(l->entries[slot].url,p);
    return token;
}
static const char *lookup(const XHyperlinks *l,uint32_t token)
{
    unsigned i;
    if(!l || token==XHYPERLINK_INVALID) return NULL;
    for(i=0;i<XHYPERLINK_ENTRIES;++i)
        if(l->entries[i].token==token) return l->entries[i].url;
    return NULL;
}
static XTermCell at(const XTermScreen *s,size_t p)
{ return screen_line_cell(s,(unsigned)(p/s->columns),(unsigned)(p%s->columns)); }
static int ascii_cell(XTermCell c)
{ return !c.link_id && c.width==1 && c.length<=1 && c.ch>32 && c.ch<127 && !strchr("\"<>`",(int)c.ch); }
static int ambiguous_boundary(XTermCell c)
{ return c.ch>=127 || c.width!=1 || c.length>1; }
/* Move within a logical line, skipping only the terminal's wide-wrap pad. */
static int step(const XTermScreen *s,size_t p,int forward,size_t *out)
{
    size_t total=(size_t)(s->history_count+s->rows)*s->columns,q;
    if(forward) {
        if(p+1>=total) return 0;
        if((p+1)%s->columns==0 && !at(s,p).wrap) return 0;
        q=p+1;
        if(q%s->columns==s->columns-1 && at(s,q).wrap==2) {
            if(q+1>=total) return 0;
            ++q;
        }
    } else {
        if(!p) return 0;
        q=p-1;
        if(p%s->columns==0) {
            unsigned wrap=at(s,q).wrap;
            if(!wrap) return 0;
            if(wrap==2) { if(!q) return 0; --q; }
        }
    }
    *out=q; return 1;
}
static size_t trim(char *p,size_t n)
{
    while(n) {
        char c=p[n-1],open=0; size_t i; int balance=0;
        if(strchr(".,;:!?",c)) { --n; continue; }
        if(c==')') open='('; else if(c==']') open='['; else if(c=='}') open='{';
        if(!open) break;
        for(i=0;i<n;++i) { if(p[i]==open) ++balance; if(p[i]==c) --balance; }
        if(balance>=0) break;
        --n;
    }
    p[n]=0; return n;
}
int hyperlink_at(const XTermScreen *s,unsigned r,unsigned c,XHyperlinkHit *h)
{
    size_t pos,first,last,q,n=0,offset=0,i;
    char candidate[XHYPERLINK_LIMIT]; XTermCell cell; const char *url;
    if(!s || !h || r>=s->rows || c>=s->columns) return 0;
    pos=(size_t)(s->history_count-s->view_offset+r)*s->columns+c;
    cell=at(s,pos);
    if(cell.link_id) {
        url=lookup(s->links,cell.link_id);
        if(!url) return 0;
        memset(h,0,sizeof *h); strcpy(h->url,url); h->token=cell.link_id;
        h->first=h->last=pos; h->generation=s->buffer_generation; return 1;
    }
    if(!ascii_cell(cell) || cell.wrap==2) return 0;
    first=pos;
    while(step(s,first,0,&q) && ascii_cell(at(s,q))) {
        first=q; if(++n>=XHYPERLINK_LIMIT) return 0;
    }
    if(step(s,first,0,&q) && ambiguous_boundary(at(s,q))) return 0;
    n=0; last=first;
    for(;;) {
        if(n+1>=XHYPERLINK_LIMIT) return 0;
        candidate[n++]=(char)at(s,last).ch;
        if(!step(s,last,1,&q)) break;
        if(!ascii_cell(at(s,q))) {
            if(ambiguous_boundary(at(s,q))) return 0;
            break;
        }
        last=q;
    }
    candidate[n]=0;
    while(offset<n && strchr("([{",candidate[offset])) ++offset;
    n=offset+trim(candidate+offset,n-offset);
    if(n<=offset || !http_target(candidate+offset,0)) return 0;
    last=first;
    for(i=0;i<n;++i) {
        if(i==offset) first=last;
        if(i+1<n) { if(!step(s,last,1,&q)) return 0; last=q; }
    }
    if(pos<first || pos>last) return 0;
    memset(h,0,sizeof *h); strcpy(h->url,candidate+offset);
    h->first=first; h->last=last; h->generation=s->buffer_generation;
    return 1;
}
int hyperlink_contains(const XTermScreen *s,const XHyperlinkHit *h,unsigned r,unsigned c)
{
    XHyperlinkHit current;
    size_t pos; XTermCell cell; const char *url;
    if(!s || !h || r>=s->rows || c>=s->columns || h->generation!=s->buffer_generation) return 0;
    pos=(size_t)(s->history_count-s->view_offset+r)*s->columns+c;
    if(h->token) {
        cell=at(s,pos);
        if(cell.link_id!=h->token) return 0;
        url=lookup(s->links,h->token);
        return url && !strcmp(url,h->url);
    }
    if(pos<h->first || pos>h->last) return 0;
    if(!hyperlink_at(s,r,c,&current) || current.generation!=h->generation ||
       current.token!=h->token || strcmp(current.url,h->url)) return 0;
    return h->token || (current.first==h->first && current.last==h->last);
}
