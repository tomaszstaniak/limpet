/* read-cancel: a console read taken back with XTY_ACTION_CANCEL
 * (sdk/include/limpet/tty_control.h), as hardcode does at exit.
 *
 * In raw mode it sends one ACTION_READ on Input(), waits a little, sends
 * XTY_ACTION_CANCEL naming that packet, and prints both answers in the
 * order they came. Then it asks for a key and reads it with a plain Read,
 * which shows whether the cancelled read took anything.
 *
 * Expected: Limpet with the cancel, the read -1/304 (ERROR_BREAK) first,
 * then the cancel TRUE; a handler without it (CON:, older Limpet) the
 * cancel FALSE/209 (ERROR_ACTION_NOT_KNOWN) while the read stays out, and
 * then the key typed is the read's. */
#include "../sdk/include/limpet/tty_control.h"
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    BPTR in = Input();
    struct FileHandle *fh = BADDR(in);
    struct MsgPort *port = CreateMsgPort();
    struct DosPacket *read = AllocDosObject(DOS_STDPKT, NULL);
    struct DosPacket *cancel = AllocDosObject(DOS_STDPKT, NULL);
    static unsigned char buf[64];
    int read_out = 1, cancel_out = 1, order = 0, ticks;
    LONG rr1 = 0, rr2 = 0, cr1 = 0, cr2 = 0;
    int read_pos = 0, cancel_pos = 0;

    if (!port || !read || !cancel || !fh || !fh->fh_Type) { puts("FAIL setup"); return 20; }
    SetMode(in, 1);
    read->dp_Type = ACTION_READ;
    read->dp_Arg1 = fh->fh_Arg1;
    read->dp_Arg2 = (SIPTR)buf;
    read->dp_Arg3 = sizeof buf;
    SendPkt(read, fh->fh_Type, port);
    Delay(10);
    cancel->dp_Type = XTY_ACTION_CANCEL;
    cancel->dp_Arg1 = fh->fh_Arg1;
    cancel->dp_Arg2 = (SIPTR)read;
    cancel->dp_Arg3 = 0;
    cancel->dp_Arg4 = XTY_PACKET_MAGIC;
    cancel->dp_Arg5 = 0;
    SendPkt(cancel, fh->fh_Type, port);
    /* The cancel's answer always comes; the read's only if it was taken
       back (or answered meanwhile). */
    for (ticks = 0; cancel_out && ticks < 250; ticks++) {
        struct Message *m;
        while ((m = GetMsg(port))) {
            if (m == read->dp_Link) { read_out = 0; rr1 = read->dp_Res1; rr2 = read->dp_Res2; read_pos = ++order; }
            else if (m == cancel->dp_Link) { cancel_out = 0; cr1 = cancel->dp_Res1; cr2 = cancel->dp_Res2; cancel_pos = ++order; }
        }
        if (cancel_out) Delay(1);
    }
    {
        struct Message *m;
        while ((m = GetMsg(port))) if (m == read->dp_Link) { read_out = 0; rr1 = read->dp_Res1; rr2 = read->dp_Res2; read_pos = ++order; }
    }
    SetMode(in, 0);
    if (cancel_out) { puts("FAIL the cancel was not answered in 5 s"); return 20; }
    printf("cancel: res1 %ld res2 %ld (answer %d)\n", (long)cr1, (long)cr2, cancel_pos);
    if (!read_out) printf("read: res1 %ld res2 %ld (answer %d)\n", (long)rr1, (long)rr2, read_pos);
    else puts("read: still out");
    if (cr1 == DOSTRUE)
        printf("%s taken back: the read was answered %s the cancel\n",
               !read_out && rr1 == -1 && rr2 == ERROR_BREAK && read_pos < cancel_pos ? "PASS" : "FAIL",
               read_pos < cancel_pos ? "before" : "after");
    else printf("INFO not taken back (res2 %ld): the handler has no cancel\n", (long)cr2);
    fflush(stdout);

    puts("type one key and Return");
    fflush(stdout);
    if (read_out) {
        /* The read is still the handler's: its answer is the key. */
        struct Message *m;
        WaitPort(port);
        while (!(m = GetMsg(port)) || m != read->dp_Link) if (!m) WaitPort(port);
        printf("the pending read got %ld byte(s): '%.*s'\n", (long)read->dp_Res1,
               read->dp_Res1 > 0 ? (int)read->dp_Res1 : 0, (const char *)buf);
    } else {
        char line[32];
        LONG n = Read(in, line, sizeof line - 1);
        printf("%s the key reached a plain Read: %ld byte(s)\n", n > 0 ? "PASS" : "FAIL", (long)n);
    }
    FreeDosObject(DOS_STDPKT, cancel);
    FreeDosObject(DOS_STDPKT, read);
    DeleteMsgPort(port);
    return 0;
}
