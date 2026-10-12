/* Guest check for file drop (docs/file-drop.md): what a program receives
 * from the Shell for the arguments Limpet wrote. Each argument is printed
 * twice, as the C start-up split it (argv) and as ReadArgs read it
 * (FILES/M), between brackets, with whether the path locks. The two must
 * agree with each other and with the object dropped. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    IPTR args[1] = { 0 };
    struct RDArgs *rd;
    int i;
    printf("argc=%d\n", argc - 1);
    for (i = 1; i < argc; i++) {
        BPTR l = Lock((CONST_STRPTR)argv[i], SHARED_LOCK);
        printf("argv[%d]=[%s] %s\n", i, argv[i], l ? "exists" : "MISSING");
        if (l) UnLock(l);
    }
    rd = ReadArgs((CONST_STRPTR)"FILES/M", (SIPTR *)args, NULL);
    if (!rd) { printf("ReadArgs failed: %ld\n", (long)IoErr()); return 10; }
    for (i = 0; args[0] && ((STRPTR *)args[0])[i]; i++) {
        const char *s = (const char *)((STRPTR *)args[0])[i];
        printf("readargs[%d]=[%s]%s\n", i + 1, s, i + 1 < argc && !strcmp(s, argv[i + 1]) ? " same" : " DIFFERS");
    }
    FreeArgs(rd);
    return 0;
}
