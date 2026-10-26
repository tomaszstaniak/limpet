/* Test setup: make OpenURL launch one command for every web link, for the
 * running system only (ENV:, never ENVARC:; gone after a reboot).
 *   openurl-set-browser "RAM:lt/link-logger %u"
 * The first browser entry gets the command and is enabled, the others are
 * disabled, and links are launched, not sent over ARexx. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <libraries/openurl.h>
#include <defines/openurl.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    struct Library *OpenURLBase;
    struct URL_Prefs *prefs;
    struct URL_BrowserNode *b;
    struct TagItem get[]={{URL_GetPrefs_Mode,URL_GetPrefs_Mode_InUse},{TAG_DONE,0}};
    struct TagItem set[]={{URL_SetPrefs_Save,FALSE},{TAG_DONE,0}};
    int first=1,ok;
    if(argc!=2 || strlen(argv[1])>=PATH_LEN) { fputs("Usage: openurl-set-browser COMMAND\n",stderr); return 20; }
    OpenURLBase=OpenLibrary((CONST_STRPTR)OPENURLNAME,7);
    if(!OpenURLBase) { puts("openurl.library 7+: unavailable"); return 20; }
    prefs=URL_GetPrefsA(get);
    if(!prefs || prefs->up_Version!=PREFS_VERSION) { puts("prefs: unavailable"); CloseLibrary(OpenURLBase); return 20; }
    for(b=(struct URL_BrowserNode *)prefs->up_BrowserList.mlh_Head;b->ubn_Node.mln_Succ;
        b=(struct URL_BrowserNode *)b->ubn_Node.mln_Succ) {
        if(first) {
            strcpy(b->ubn_Name,"link-logger");
            strcpy(b->ubn_Path,argv[1]);
            b->ubn_Port[0]=0;
            b->ubn_Flags&=~UNF_DISABLED;
            first=0;
        } else b->ubn_Flags|=UNF_DISABLED;
    }
    prefs->up_DefLaunch=TRUE;
    ok=!first && URL_SetPrefsA(prefs,set);
    printf("openurl %u.%u browser=%s set=%s\n",(unsigned)OpenURLBase->lib_Version,
        (unsigned)OpenURLBase->lib_Revision,argv[1],ok?"ok":"FAILED");
    URL_FreePrefsA(prefs,NULL);
    CloseLibrary(OpenURLBase);
    return ok?0:20;
}
