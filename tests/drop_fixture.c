/* Guest fixture for file drop: a fresh drawer whose objects have names the
 * Shell and ReadArgs read specially, each with a default icon, so they
 * can be dragged from Wanderer. Also an icon whose object is missing, and
 * a name in Latin-1 (not UTF-8). Usage: drop-fixture DRAWER */
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>

static const char *files[] = {
    "plain.txt", "with space.txt", "quote\"d.txt", "st*ar.txt", "$HOME.txt",
    "back`tick`.txt", "semi;colon.txt", "par(en)#?.txt", "z\xc5\xbc\xc3\xb3\xc5\x82w.txt",
    "caf\xe9.txt", NULL
};

static int icon(const char *path, LONG type)
{
    struct DiskObject *d = GetDefDiskObject(type);
    int ok = d && PutDiskObject((CONST_STRPTR)path, d);
    if (d) FreeDiskObject(d);
    return ok;
}

int main(int argc, char **argv)
{
    char p[512];
    BPTR l, f;
    int i;
    if (argc != 2) { puts("usage: drop-fixture DRAWER"); return 10; }
    if ((l = Lock((CONST_STRPTR)argv[1], SHARED_LOCK))) { UnLock(l); puts("drawer exists"); return 10; }
    if (!(l = CreateDir((CONST_STRPTR)argv[1]))) { puts("cannot create the drawer"); return 10; }
    UnLock(l);
    icon(argv[1], WBDRAWER);
    for (i = 0; files[i]; i++) {
        snprintf(p, sizeof p, "%s/%s", argv[1], files[i]);
        if (!(f = Open((CONST_STRPTR)p, MODE_NEWFILE))) { printf("cannot create [%s]\n", p); return 10; }
        FPuts(f, (CONST_STRPTR)"dropped\n");
        Close(f);
        printf("%s [%s]\n", icon(p, WBPROJECT) ? "made" : "made, NO ICON", p);
    }
    snprintf(p, sizeof p, "%s/drawer with space", argv[1]);
    if ((l = CreateDir((CONST_STRPTR)p))) UnLock(l);
    printf("%s [%s]\n", icon(p, WBDRAWER) ? "made" : "made, NO ICON", p);
    /* An icon whose object is not there. */
    snprintf(p, sizeof p, "%s/ghost.txt", argv[1]);
    printf("%s [%s]\n", icon(p, WBPROJECT) ? "icon only" : "NO ICON", p);
    return 0;
}
