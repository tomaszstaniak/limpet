#include "file_drop.h"
#include "utf8proc.h"
#include <string.h>

/* Unquoted, these separate arguments (space, tab, =), start a comment (;),
   redirect (< >) or expand ($ `) in the Shell, or escape the next
   character (*). The pattern characters are quoted as Tab completion does. */
static int needs_quotes(unsigned char c)
{
    return c && strchr(" \t\"*<>;=#?()[]|$`", c) != NULL;
}

/* Inside quotes the Shell still expands $ and `, and ReadArgs ends at ". */
static int needs_escape(unsigned char c)
{
    return c=='*' || c=='"' || c=='$' || c=='`';
}

static int writable(const char *s, size_t n)
{
    size_t at=0;
    while(at<n) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t used=utf8proc_iterate((const utf8proc_uint8_t *)s+at,(utf8proc_ssize_t)(n-at),&cp);
        if(used<=0 || cp<32 || (cp>=127 && cp<=159)) return 0;
        at+=(size_t)used;
    }
    return 1;
}

size_t file_drop_quote(const char *path, char *out, size_t capacity)
{
    size_t n=strlen(path), i, used=0;
    int quote=0;
    if(!n || !writable(path,n)) return 0;
    for(i=0;i<n;i++) if(needs_quotes((unsigned char)path[i])) quote=1;
    if(quote && used<capacity) out[used++]='"';
    for(i=0;i<n;i++) {
        if(quote && needs_escape((unsigned char)path[i])) {
            if(used+1>=capacity) return 0;
            out[used++]='*';
        }
        if(used+1>=capacity) return 0;
        out[used++]=path[i];
    }
    if(quote) {
        if(used+1>=capacity) return 0;
        out[used++]='"';
    }
    if(used>=capacity) return 0;
    out[used]=0;
    return used;
}

int file_drop_shell_cuts(const char *line, size_t length)
{
    size_t i;
    int naive=0, quoted=0;
    for(i=0;i<length;i++) {
        char c=line[i];
        if(c=='"') naive=!naive;
        if(c=='*' && i+1<length) {             /* the escaped character is text */
            if(line[i+1]=='"') naive=!naive;
            ++i;
            continue;
        }
        if(c=='"') quoted=!quoted;
        else if(c==';' && naive!=quoted) return 1;
        else if(c==';' && !quoted) return 0;   /* a real comment: the rest is not read */
    }
    return 0;
}

static int space(char c) { return c==' ' || c=='\t'; }

XDropResult file_drop_insert(XLineEditor *e, const char *const *paths, size_t count, size_t *failed)
{
    char text[XLINE_CAPACITY], one[XLINE_CAPACITY];
    size_t used=0, i, n, anchor=e->anchor;
    int selecting=e->selecting;
    *failed=count;
    if(e->searching) return XDROP_SEARCHING;
    if(count>XDROP_MAX_FILES) return XDROP_TOO_MANY;
    if(!count) return XDROP_OK;
    if(e->cursor>0 && !space(e->text[e->cursor-1])) text[used++]=' ';
    for(i=0;i<count;i++) {
        if(!writable(paths[i],strlen(paths[i])) || !*paths[i]) { *failed=i; return XDROP_BAD_NAME; }
        if(!(n=file_drop_quote(paths[i],one,sizeof one)) || used+n+2>sizeof text) return XDROP_TOO_LONG;
        if(i) text[used++]=' ';
        memcpy(text+used,one,n); used+=n;
    }
    if(e->cursor==e->length || !space(e->text[e->cursor])) text[used++]=' ';
    if(e->length+used>=XLINE_CAPACITY) return XDROP_TOO_LONG;
    {
        char after[XLINE_CAPACITY];
        memcpy(after,e->text,e->cursor);
        memcpy(after+e->cursor,text,used);
        memcpy(after+e->cursor+used,e->text+e->cursor,e->length-e->cursor);
        if(file_drop_shell_cuts(after,e->length+used) && !file_drop_shell_cuts(e->text,e->length))
            return XDROP_SHELL_CUT;
    }
    /* At the cursor, not over a selection: the selected text stays. */
    e->selecting=0; e->anchor=e->cursor;
    if(!line_editor_insert(e,text,used)) {
        e->selecting=selecting; e->anchor=anchor;
        return XDROP_TOO_LONG;
    }
    return XDROP_OK;
}
