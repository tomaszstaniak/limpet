/* Test stand-in for a browser or a default tool: records what it was
 * started with, so a link test checks the exact hand-over and not whether
 * a browser renders the page.
 *   link-logger ARGS...           (as an OpenURL browser: Path "link-logger %u")
 *   started from Workbench        (as a project icon's default tool)
 *   link-logger --icon FILE TOOL  write FILE.info, a project with TOOL as its default tool
 * Each start appends one line to the file named by the global variable
 * LinkLog: "shell <arg>|<arg>..." or "wb <full name>|...". */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <string.h>
#include <stdio.h>

static char line[4096];
static void add(const char *s)
{
    size_t n=strlen(line);
    if(n && n+1<sizeof line) { line[n++]='|'; line[n]=0; }
    strncat(line,s,sizeof line-strlen(line)-1);
}
static int make_icon(const char *file,const char *tool)
{
    struct Library *IconBase=OpenLibrary((CONST_STRPTR)"icon.library",0);
    struct DiskObject *d;
    int ok=0;
    if(!IconBase) return 20;
    d=GetDefDiskObject(WBPROJECT);
    if(d) {
        d->do_DefaultTool=(STRPTR)tool;
        ok=PutDiskObject((CONST_STRPTR)file,d);
        FreeDiskObject(d);
    }
    CloseLibrary(IconBase);
    return ok?0:20;
}
int main(int argc,char **argv)
{
    char out[512],name[1024];
    BPTR f;
    int i;
    if(argc==4 && !strcmp(argv[1],"--icon")) return make_icon(argv[2],argv[3]);
    if(GetVar((CONST_STRPTR)"LinkLog",(STRPTR)out,sizeof out,GVF_GLOBAL_ONLY)<=0) return 20;
    if(argc) {
        strcpy(line,"shell ");
        for(i=1;i<argc;++i) { if(i>1) add(""); strncat(line,argv[i],sizeof line-strlen(line)-1); }
    } else {
        struct WBStartup *wb=(struct WBStartup *)argv;
        strcpy(line,"wb ");
        for(i=1;i<wb->sm_NumArgs;++i) {
            name[0]=0;
            if(wb->sm_ArgList[i].wa_Lock) NameFromLock(wb->sm_ArgList[i].wa_Lock,(STRPTR)name,sizeof name);
            AddPart((STRPTR)name,(CONST_STRPTR)wb->sm_ArgList[i].wa_Name,sizeof name);
            if(i>1) add("");
            strncat(line,name,sizeof line-strlen(line)-1);
        }
    }
    f=Open((CONST_STRPTR)out,MODE_READWRITE);
    if(!f) return 20;
    Seek(f,0,OFFSET_END);
    strncat(line,"\n",sizeof line-strlen(line)-1);
    Write(f,line,(LONG)strlen(line));
    Close(f);
    return 0;
}
