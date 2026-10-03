/* Read-only OpenURL configuration diagnostic. Never launches or changes prefs. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/rdargs.h>
#include <dos/dosextens.h>
#include <libraries/openurl.h>
#include <defines/openurl.h>
#include <stdio.h>
#include <string.h>

static void probe_path(const struct URL_BrowserNode *browser)
{
    char path[PATH_LEN+1],executable[PATH_LEN+1],error_text[256];
    const char *end=memchr(browser->ubn_Path,0,PATH_LEN);
    struct CSource source;
    struct Process *process=(struct Process *)FindTask(NULL);
    APTR previous_window;
    BPTR lock;
    LONG item,error;
    if(!end) { puts("  executable: rejected unterminated preference path"); return; }
    memcpy(path,browser->ubn_Path,(size_t)(end-browser->ubn_Path)+1);
    source.CS_Buffer=(UBYTE *)path;
    source.CS_Length=(LONG)strlen(path);
    source.CS_CurChr=0;
    item=ReadItem((STRPTR)executable,sizeof executable,&source);
    if((item!=ITEM_QUOTED && item!=ITEM_UNQUOTED) || !executable[0]) {
        printf("  executable: no usable first command token; ReadItem=%ld\n",(long)item);
        return;
    }
    printf("  executable_token=%s\n",executable);
    previous_window=process->pr_WindowPtr;
    process->pr_WindowPtr=(APTR)-1; /* Missing assigns must not open requesters. */
    lock=Lock((CONST_STRPTR)executable,ACCESS_READ);
    error=lock?0:IoErr();
    process->pr_WindowPtr=previous_window;
    if(lock) {
        puts("  Lock=present (existence only; does not prove executable/launchable)");
        UnLock(lock);
    } else {
        error_text[0]=0;
        Fault(error,NULL,(STRPTR)error_text,sizeof error_text);
        printf("  Lock=unresolved IoErr=%ld detail=%s\n",(long)error,error_text);
    }
    if(!strchr(executable,':') && !strchr(executable,'/'))
        puts("  note: bare command checked in current directory; Shell PATH is not searched");
}

int main(int argc,char **argv)
{
    struct Library *OpenURLBase;
    struct URL_Prefs *prefs;
    struct URL_BrowserNode *browser;
    struct TagItem tags[]={{URL_GetPrefs_Mode,URL_GetPrefs_Mode_InUse},{TAG_DONE,0}};
    unsigned count=0,enabled=0;
    (void)argv;
    if(argc!=1) { fputs("Usage: openurl-probe (read-only; no arguments)\n",stderr); return 20; }
    puts("OpenURL diagnostic: read-only; no launch and no preference changes");
    OpenURLBase=OpenLibrary((CONST_STRPTR)OPENURLNAME,0);
    if(!OpenURLBase) { puts("openurl.library: unavailable"); return 5; }
    printf("openurl.library: version=%u revision=%u\n",
        (unsigned)OpenURLBase->lib_Version,(unsigned)OpenURLBase->lib_Revision);
    if(OpenURLBase->lib_Version<7) {
        puts("preferences: API requires library version 7 or newer");
        CloseLibrary(OpenURLBase); return 5;
    }
    prefs=URL_GetPrefsA(tags);
    if(!prefs) { puts("URL_GetPrefsA(InUse): failed"); CloseLibrary(OpenURLBase); return 5; }
    printf("preferences: source=InUse version=%u flags=0x%lx default_launch=%lu\n",
        (unsigned)prefs->up_Version,(unsigned long)prefs->up_Flags,(unsigned long)prefs->up_DefLaunch);
    if(prefs->up_Version!=PREFS_VERSION) {
        puts("preferences: unsupported layout version");
        URL_FreePrefsA(prefs,NULL); CloseLibrary(OpenURLBase); return 5;
    }
    for(browser=(struct URL_BrowserNode *)prefs->up_BrowserList.mlh_Head;
        browser->ubn_Node.mln_Succ;
        browser=(struct URL_BrowserNode *)browser->ubn_Node.mln_Succ) {
        int disabled=(browser->ubn_Flags&UNF_DISABLED)!=0;
        ++count; if(!disabled) ++enabled;
        printf("browser[%u]: disabled=%d flags=0x%lx name=%.*s\n",count,disabled,
            (unsigned long)browser->ubn_Flags,NAME_LEN,browser->ubn_Name);
        printf("  path=%.*s\n  port=%.*s\n  open_command=%.*s\n  open_new_window=%.*s\n",
            PATH_LEN,browser->ubn_Path,PORT_LEN,browser->ubn_Port,
            OPENURLCMD_LEN,browser->ubn_OpenURLCmd,OPENURLWCMD_LEN,browser->ubn_OpenURLWCmd);
        probe_path(browser);
    }
    printf("OPENURL PROBE COMPLETE browsers=%u enabled=%u\n",count,enabled);
    URL_FreePrefsA(prefs,NULL); CloseLibrary(OpenURLBase);
    return 0;
}
