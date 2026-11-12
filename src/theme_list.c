#include "theme_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower(int c) { return c>='A' && c<='Z'?c+32:c; }
static int compare(const char *a,const char *b)
{
    for(;*a && *b;++a,++b) {
        int d=lower((unsigned char)*a)-lower((unsigned char)*b);
        if(d) return d;
    }
    return (unsigned char)*a-(unsigned char)*b;
}
static int ends(const char *name,const char *tail)
{
    size_t n=strlen(name),t=strlen(tail);
    return n>=t && !compare(name+n-t,tail);
}
static int listed(const char *name)
{
    return name[0] && name[0]!='.' && !ends(name,".info") && !ends(name,".bak") && !ends(name,".new");
}
static int order(const void *a,const void *b)
{
    const XThemeEntry *x=a,*y=b;
    int d=compare(x->name,y->name);
    /* The user's file before the program's of the same name. */
    return d?d:(int)y->user-(int)x->user;
}

unsigned theme_list_merge(XThemeEntry *out,unsigned capacity,const XThemeEntry *bundled,unsigned bundled_count,
                          const XThemeEntry *user,unsigned user_count)
{
    unsigned n=0,i,kept=0;
    for(i=0;i<user_count && n<capacity;++i) if(listed(user[i].name)) { out[n]=user[i]; out[n++].user=1; }
    for(i=0;i<bundled_count && n<capacity;++i) if(listed(bundled[i].name)) { out[n]=bundled[i]; out[n++].user=0; }
    qsort(out,n,sizeof *out,order);
    for(i=0;i<n;++i) {
        if(kept && !compare(out[kept-1].name,out[i].name)) continue;
        out[kept++]=out[i];
    }
    return kept;
}

static const char *const endings[]={".theme",".conf",".itermcolors"};

void theme_list_label(char *out,size_t size,const char *name)
{
    size_t n=strlen(name);
    unsigned i;
    for(i=0;i<3;++i) if(ends(name,endings[i]) && n>strlen(endings[i])) { n-=strlen(endings[i]); break; }
    snprintf(out,size,"%.*s",(int)n,name);
}

int theme_list_find(const XThemeEntry *list,unsigned count,const char *value)
{
    char with[XTHEME_NAME_MAX+16];
    unsigned i,e;
    for(i=0;i<count;++i) if(!compare(list[i].name,value)) return (int)i;
    for(e=0;e<3;++e) {
        snprintf(with,sizeof with,"%s%s",value,endings[e]);
        for(i=0;i<count;++i) if(!compare(list[i].name,with)) return (int)i;
    }
    return -1;
}
