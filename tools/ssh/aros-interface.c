#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/miami.h>
#include <stdlib.h>
#include <string.h>
char *xt_ssh_home_dir(void)
{
    const char *configured=getenv("HOME");
    char path[1024];
    if(configured && *configured) return strdup(configured);
    if(!NameFromLock(GetProgramDir(),path,sizeof path)) return NULL;
    return strdup(path);
}
/* Interface-name lookup is a Miami API in mainline, not a POSIX symbol. */
unsigned int xt_ssh_if_nametoindex(const char *name)
{
    struct Library *MiamiBase=OpenLibrary("miami.library",0);
    unsigned int index=0;
    if(MiamiBase) {
        index=if_nametoindex((char *)name);
        CloseLibrary(MiamiBase);
    }
    return index;
}
