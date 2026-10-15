/* Guest test tool: press and release one key through input.device, as a
 * keyboard does, for keys a test driver cannot send (QMP's backslash does
 * not reach AROS One's keymap). It goes to the active window.
 *     key-inject RAWCODE [QUALIFIER] [DELAY_TICKS]
 * e.g. key-inject 0x0d 0x0008 150  is Ctrl+\ (US keymap) after 3 s.
 * Prints what the key maps to with the qualifier left out. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/keymap.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct MsgPort *port;
    struct IOStdReq *io;
    struct InputEvent ev;
    char mapped[8];
    LONG n;
    UWORD code, qual;
    if (argc < 2) { PutStr((CONST_STRPTR)"Usage: key-inject RAWCODE [QUALIFIER] [DELAY_TICKS]\n"); return 20; }
    code = (UWORD)strtoul(argv[1], NULL, 0);
    qual = argc > 2 ? (UWORD)strtoul(argv[2], NULL, 0) : 0;
    memset(&ev, 0, sizeof ev);
    ev.ie_Class = IECLASS_RAWKEY; ev.ie_Code = code;
    n = MapRawKey(&ev, (STRPTR)mapped, sizeof mapped - 1, NULL);
    printf("rawkey 0x%02x maps to %ld byte(s)%s%c\n", code, (long)n, n == 1 ? ": " : "", n == 1 ? mapped[0] : ' ');
    if (argc > 3) Delay(strtol(argv[3], NULL, 0));
    port = CreateMsgPort();
    io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof *io) : NULL;
    if (!io || OpenDevice((CONST_STRPTR)"input.device", 0, (struct IORequest *)io, 0)) {
        if (io) DeleteIORequest((struct IORequest *)io);
        if (port) DeleteMsgPort(port);
        PutStr((CONST_STRPTR)"key-inject: no input.device\n"); return 20;
    }
    ev.ie_Qualifier = qual;
    io->io_Command = IND_WRITEEVENT; io->io_Data = &ev; io->io_Length = sizeof ev;
    DoIO((struct IORequest *)io);
    ev.ie_Code = code | IECODE_UP_PREFIX;
    DoIO((struct IORequest *)io);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io); DeleteMsgPort(port);
    return 0;
}
