#include "line_editor.h"
#include "utf8proc.h"
#include <string.h>

/* Re-scan a bounded line rather than caching boundaries that an insertion of
 * a combining mark, ZWJ or regional indicator can invalidate on either side. */
static size_t boundaries(const char *text, size_t length, size_t *points)
{
    size_t pos=0, count=1, cluster=0;
    utf8proc_int32_t previous=0, state=0;
    points[0]=0;
    while (pos<length) {
        utf8proc_int32_t scalar;
        utf8proc_ssize_t used=utf8proc_iterate(
            (const utf8proc_uint8_t *)text+pos,
            (utf8proc_ssize_t)(length-pos), &scalar);
        if (used<=0 || scalar<32 || (scalar>=127 && scalar<=159)) return 0;
        if (pos && utf8proc_grapheme_break_stateful(previous,scalar,&state)) {
            points[count++]=pos;
            cluster=0;
        }
        if (++cluster>XLINE_CLUSTER_LIMIT) return 0;
        previous=scalar;
        pos+=(size_t)used;
    }
    if (length) points[count++]=length;
    return count;
}

static int replace(XLineEditor *s,size_t start,size_t end,
                   const char *text,size_t n)
{
    char candidate[XLINE_CAPACITY];
    size_t points[XLINE_CAPACITY+1], count, i;
    size_t kept=s->length-(end-start), target=start+n;
    if (n>XLINE_CAPACITY-1-kept || (n && !text)) return 0;
    memcpy(candidate,s->text,start);
    if (n) memcpy(candidate+start,text,n);
    memcpy(candidate+start+n,s->text+end,s->length-end);
    candidate[kept+n]=0;
    count=boundaries(candidate,kept+n,points);
    if (!count) return 0;
    /* An edit can join a cluster with the suffix. Put the cursor after the
     * resulting cluster, never between its scalars. */
    for (i=0;i<count && points[i]<target;++i) {}
    memcpy(s->text,candidate,kept+n+1);
    s->length=kept+n;
    s->cursor=points[i];
    s->selecting=0; s->anchor=s->cursor;
    return 1;
}

int line_editor_insert(XLineEditor *s, const char *text, size_t n)
{
    if (!n) return 1;
    if(s->selecting && s->anchor!=s->cursor)
        return replace(s,s->anchor<s->cursor?s->anchor:s->cursor,
                         s->anchor>s->cursor?s->anchor:s->cursor,text,n);
    return replace(s,s->cursor,s->cursor,text,n);
}

static void recall(XLineEditor *s)
{
    if (!s->browsing) {
        strcpy(s->text,s->draft);
        s->length=strlen(s->text);
        s->cursor=s->draft_cursor;
        s->anchor=s->draft_anchor; s->selecting=s->draft_selecting;
    } else {
        size_t slot=(s->head+XLINE_HISTORY-s->browsing)%XLINE_HISTORY;
        strcpy(s->text,s->history[slot]);
        s->cursor=s->length=strlen(s->text);
        s->anchor=s->cursor; s->selecting=0;
    }
}

int line_editor_action(XLineEditor *s, XLineAction action)
{
    size_t points[XLINE_CAPACITY+1], count, index;
    if(action>=XLINE_SELECT_LEFT && action<=XLINE_SELECT_END) {
        XLineAction movement=(XLineAction)(XLINE_LEFT+action-XLINE_SELECT_LEFT);
        size_t anchor=s->selecting?s->anchor:s->cursor;
        int changed;
        s->selecting=0;
        changed=line_editor_action(s,movement);
        s->anchor=anchor; s->selecting=s->cursor!=anchor;
        return changed;
    }
    if(s->selecting && s->cursor!=s->anchor) {
        size_t start=s->cursor<s->anchor?s->cursor:s->anchor;
        size_t end=s->cursor>s->anchor?s->cursor:s->anchor;
        if(action==XLINE_BACKSPACE || action==XLINE_DELETE)
            return replace(s,start,end,NULL,0);
        if(action==XLINE_LEFT || action==XLINE_RIGHT) {
            s->cursor=action==XLINE_LEFT?start:end;
            s->anchor=s->cursor; s->selecting=0; return 1;
        }
    }
    switch (action) {
    case XLINE_PREVIOUS:
        if (s->browsing==s->count) return 0;
        if (!s->browsing) {
            strcpy(s->draft,s->text);
            s->draft_cursor=s->cursor;
            s->draft_anchor=s->anchor; s->draft_selecting=s->selecting;
        }
        ++s->browsing; recall(s); return 1;
    case XLINE_NEXT:
        if (!s->browsing) return 0;
        --s->browsing; recall(s); return 1;
    case XLINE_CLEAR:
        if (!s->length && !s->browsing) return 0;
        s->text[0]=s->draft[0]=0;
        s->length=s->cursor=s->browsing=s->draft_cursor=0;
        s->anchor=s->draft_anchor=0; s->selecting=s->draft_selecting=0;
        return 1;
    case XLINE_HOME:
        s->selecting=0; s->anchor=0;
        if (!s->cursor) return 0;
        s->cursor=0; return 1;
    case XLINE_END:
        s->selecting=0; s->anchor=s->length;
        if (s->cursor==s->length) return 0;
        s->cursor=s->length; return 1;
    case XLINE_LEFT: case XLINE_RIGHT: case XLINE_BACKSPACE: case XLINE_DELETE:
        break;
    default: return 0;
    }
    count=boundaries(s->text,s->length,points);
    for (index=0;index<count && points[index]!=s->cursor;++index) {}
    if (index==count) return 0;
    if (action==XLINE_LEFT || action==XLINE_BACKSPACE) {
        if (!index) return 0;
        if (action==XLINE_LEFT) { s->anchor=s->cursor=points[index-1]; return 1; }
        return replace(s,points[index-1],s->cursor,NULL,0);
    }
    if (index+1==count) return 0;
    if (action==XLINE_RIGHT) { s->anchor=s->cursor=points[index+1]; return 1; }
    return replace(s,s->cursor,points[index+1],NULL,0);
}

size_t line_editor_commit(XLineEditor *s, char *out, size_t capacity)
{
    size_t used=s->length+1;
    if (!out || capacity<used) return 0;
    memcpy(out,s->text,s->length); out[s->length]='\n';
    if (s->length && (!s->count || strcmp(s->text,
        s->history[(s->head+XLINE_HISTORY-1)%XLINE_HISTORY]))) {
        strcpy(s->history[s->head],s->text);
        s->head=(s->head+1)%XLINE_HISTORY;
        if (s->count<XLINE_HISTORY) ++s->count;
    }
    s->text[0]=s->draft[0]=0;
    s->length=s->cursor=s->browsing=s->draft_cursor=0;
    s->anchor=s->draft_anchor=0; s->selecting=s->draft_selecting=0;
    return used;
}
