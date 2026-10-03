#include "paste.h"
#include "utf8proc.h"
#include <string.h>
int paste_prepare(XPaste *p,const unsigned char *s,size_t n)
{
    size_t i=0,o=0; int multiline=0;
    if(!p || (!s && n)) return -1;
    if(n>PASTE_LIMIT) return -2;
    while(i<n) {
        int32_t scalar; utf8proc_ssize_t used=utf8proc_iterate(s+i,(utf8proc_ssize_t)(n-i),&scalar);
        if(used<=0 || (scalar<32 && scalar!='\t' && scalar!='\r' && scalar!='\n') ||
           (scalar>=127 && scalar<160)) return -1;
        if(scalar=='\r' || scalar=='\n') multiline=1;
        i+=(size_t)used;
    }
    for(i=0;i<n;++i) {
        if(s[i]=='\r') {
            p->bytes[o++]='\r';
            if(i+1<n && s[i+1]=='\n') ++i;
        } else p->bytes[o++]=s[i]=='\n'?'\r':s[i];
    }
    p->offset=0; p->length=o; p->confirmation=multiline;
    return 0;
}
void paste_cancel(XPaste *p) { p->length=p->offset=0;p->confirmation=0; }
void paste_confirm(XPaste *p) { p->confirmation=0; }
size_t paste_peek(const XPaste *p,const unsigned char **s,size_t n)
{
    size_t left=p->length-p->offset;
    *s=p->bytes+p->offset;
    if(p->confirmation) return 0;
    if(n>left) n=left;
    /* Do not strand a UTF-8 prefix in the session if the user cancels before
     * the next batch. Validated input needs at most three bytes backed off. */
    while(n && n<left && (p->bytes[p->offset+n]&0xc0)==0x80) --n;
    return n;
}
void paste_consume(XPaste *p,size_t n)
{
    size_t left=p->length-p->offset;
    p->offset+=n<left?n:left;
    if(p->offset==p->length) paste_cancel(p);
}
